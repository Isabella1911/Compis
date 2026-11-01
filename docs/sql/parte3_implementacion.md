# Implementación de ejecución de consultas (Parte 3)

La CLI ejecuta SQL con `exec::Session`. El catálogo, los CSV y los índices son
los de la Parte 2; ninguna consulta usa un DBMS externo. Para probar sin alterar
la base de ejemplo, los runners crean directorios temporales.

## Componentes

- `src/exec/session.{h,cpp}`: análisis de cada sentencia, despacho, catálogo
  actualizado, caché por sentencia y detención en el primer error.
- `eval_context.h`, `evaluator.{h,cpp}`: tuplas por fuente, contexto externo,
  valores de agregados y proyección para alias de ORDER BY. Una columna se
  lee exclusivamente por `{depth, source, column}`; una anotación faltante
  lanza `std::logic_error` porque representa un fallo de integración.
- `select`, `join`, `aggregate`: materializan las etapas FROM/JOIN, WHERE,
  GROUP BY, HAVING, proyección y ordenamiento estable. Los JOIN usan nested
  loops y soportan varias tablas. Los términos del WHERE que solo dependen de
  la primera fuente se aplican antes del JOIN.
- `subquery`: las consultas no correlacionadas se guardan por nodo AST
  durante una sentencia. Las correlacionadas se ejecutan con `outer` y no se
  guardan. EXISTS evalúa cardinalidad sin calcular la proyección ni ordenar.
- `planner`: divide las conjunciones, selecciona el primer término indexable,
  usa `lookupEq`/`lookupRange` y `readAt`, y comprueba todos los filtros
  residuales. Las constantes pueden incluir referencias externas o una
  subconsulta escalar. Una raíz OR, IS NULL o columna dentro de aritmética
  utiliza SeqScan. El campo `plan` incluye las subconsultas ejecutadas.
- `result_json`: serialización del contrato A.7 y tablas de consola. JSON
  conserva números, booleanos y NULL; DATE es texto ISO. En consola, FLOAT
  tiene hasta seis decimales sin ceros sobrantes.

Los NULL se ordenan como el mayor valor: al final con ASC y al inicio con
DESC. En GROUP BY todos los NULL de una clave forman un solo grupo. Sin
GROUP BY, una consulta con agregados conserva un grupo aun con entrada vacía.

## Integración

```cpp
sqlint::exec::Session session(dbDir);
auto result = session.run(*parsed.script, noExec);
std::cout << sqlint::exec::toJson(result);
```

El frontend debe haber parseado todo el script sin errores. Session llama a
`semantic::analyze` antes de cada sentencia y entrega al Mutator los hooks
reales `eval` y `matchingRows`, de modo que UPDATE y DELETE también eligen
índice. El evaluador provisional `storage/fallback_exec` se conserva para las
pruebas aisladas históricas de almacenamiento; la CLI usa `src/exec`.

Se amplió el semántico para anotar JOIN, agregados, subconsultas e índices
(SEM003, SEM005 y SEM016–SEM022). `SelectItem::source` anota `t.*`; no se
resuelven calificadores en el ejecutor. Las referencias a columnas no
agrupadas también se comprueban dentro de subconsultas correlacionadas.
`simulateSchema` comparte la simulación de CREATE/DROP TABLE e INDEX entre
Session y el verificador semántico. `--no-exec` no crea archivos ni ejecuta
expresiones.

La CLI admite `--emit catalog` con o sin archivo SQL. La salida normal y la
salida JSON contienen los resultados de ejecución y su plan. La IDE es una
parte separada del proyecto y puede consumir estos campos del contrato A.7.

RUN201 señala el paréntesis de la subconsulta escalar que devuelve varias
filas; RUN202 señala el operador de división/módulo por cero. Una sentencia
que falla en ejecución se incluye con `status: "error"`; las anteriores
mantienen su resultado y las siguientes no se ejecutan. Los errores
semánticos se agregan a diagnostics sin ejecutar la sentencia inválida.

## Verificación

```bash
make test-exec       # contratos C++ y oráculos de filas/errores/planes
make test           # semántica, almacenamiento, mutador, ejecución y snapshots
```

Los casos propios están en `tests/sql/casos/{select,join,agregados,
subconsultas,plan}/` con sus archivos `.expected`. Los nuevos errores
semánticos tienen casos en `casos/semantico/`. Los oráculos Python incluyen
las tablas de verdad completas, división entera, NULL, grupos vacíos,
HAVING/ORDER BY con agregados no proyectados, correlación a dos niveles,
paridad de veinte predicados con y sin índice, persistencia tras errores y
la ausencia de escrituras con `--no-exec`. Una prueba corrompe una fila ajena
al predicado: IndexScan obtiene el resultado sin leerla y SeqScan detecta
RUN103. Las pruebas C++ inspeccionan la caché y rechazan bindings faltantes.
