# Parte 2: estado de la implementación

Complementa a [parte2_almacenamiento_indices.md](parte2_almacenamiento_indices.md)
(el plan). Aquí está lo que ya existe, cómo lo usa la Parte 3 y en qué se
aparta del plan.

## Archivos

| Archivo | Qué hace |
|---|---|
| `src/storage/value.{h,cpp}` | `Value`, `SqlType`, `Date`, `compareValues`, `ValueLess`, `toCsvField`/`fromCsvField`, `parseDate`, `typeName`/`parseTypeName`. |
| `src/storage/csv.{h,cpp}` | `CsvReader` (RFC 4180, devuelve el offset y la línea de cada registro, tolera `\r\n`) y `formatCsvRecord`. |
| `src/storage/fs_util.{h,cpp}` | `writeFileAtomic` (`.tmp` + `rename`, cerrando antes el archivo), `ensureDir`, `readFile`, `removeFile`. |
| `src/storage/catalog.{h,cpp}` | `Catalog::load`/`save` de `metadata.txt`, búsquedas sin distinguir mayúsculas y `toJson()` para `--emit catalog`. |
| `src/storage/table_store.{h,cpp}` | `scan`, `readAt`, `append`, `rewrite`, `create` y `drop`. |
| `src/storage/mutator.{h,cpp}` | `CREATE`/`DROP TABLE`, `CREATE`/`DROP INDEX`, `INSERT`, `UPDATE` y `DELETE`, con `RUN101`, `RUN102` y `RUN104`. |
| `src/storage/fallback_exec.{h,cpp}` | **Provisional.** `eval` y `matchingRows` mínimos hasta que la Parte 3 tenga los suyos. |
| `src/index/index_file.{h,cpp}` | Lectura y escritura del `.idx`. |
| `src/index/index_manager.{h,cpp}` | `lookupEq`, `lookupRange`, `create`, `drop`, `onInsert`, `rebuild`, `removeFiles`, con caché. |
| `databases/escuela/` | Base de ejemplo y `crear.sql`, que la construye desde cero. |
| `tests/sql/run_tests.py` | Runner de la batería (`make test`, `make test-update`). |
| `tests/sql/storage/` | `make test-storage` (sin ANTLR) y `make test-mutator`. |
| `tests/sql/casos/{ddl,dml,indices,restricciones}/` | Casos de la Parte 2 para el runner. |

## Cómo lo conecta la `Session` (Parte 3)

```cpp
storage::Catalog catalog = storage::Catalog::load(dbDir);   // RUN103 si está corrupto
storage::TableStore store(dbDir);
index::IndexManager indexes(catalog, store);

storage::ExecHooks hooks;
hooks.eval = [&](const ast::Expr& e, const storage::TableDef& t, const storage::Row* row) {
    return eval(e, /* EvalContext con `row` como fuente 0 */);
};
hooks.matchingRows = [&](const storage::TableDef& t, const ast::Expr* where) {
    return matchingRows(t, where, session).rows;   // el planificador elige índice o recorrido
};
storage::Mutator mutator(catalog, store, indexes, hooks);

// Por cada sentencia que modifica, después del semántico:
storage::MutationResult r = mutator.execute(stmt);   // lanza SqlError (RUN1xx) si falla
// r.affected y r.message van al StatementResult.
```

- El `Mutator` recibe el **mismo** `Catalog` que usa el semántico: después de
  un `CREATE TABLE` la tabla ya está en memoria para la siguiente sentencia.
- Mientras no existan `eval` y `matchingRows`, `storage::fallbackHooks(store)`
  da una versión que cubre literales, columnas, aritmética, comparaciones,
  `AND`/`OR`/`NOT`, `IS NULL` e `IN (lista)`, y siempre hace recorrido.
- El planificador busca con `indexes.lookupEq(idx, valor)` o
  `indexes.lookupRange(idx, lo, loIncl, hi, hiIncl)` y lee las filas con
  `store.readAt(tabla, offsets)`. Los índices de una columna salen de
  `catalog.indexesOn(tabla, columna)`.
- `--emit catalog` (Parte 1): `catalog.toJson()` devuelve
  `{"tables": [{"name", "columns": [{"name", "type", "notNull", "primaryKey"}], "indexes": [{"name", "column"}]}]}`.

## Decisiones y diferencias con el plan

- **`fromCsvField` recibe el campo crudo**, con sus comillas, tal como está en
  el archivo. Es la única forma de distinguir el campo vacío (`NULL`) de `""`
  (cadena vacía), y así `fromCsvField(toCsvField(v)) == v` siempre.
- **`TableStore` recibe el directorio** de la base en el constructor.
- **`toCsvField` también pone comillas a un VARCHAR con tabulador**, porque el
  `.idx` usa el mismo formato con tabulador como separador.
- **`FLOAT`** se escribe con la menor cantidad de dígitos (15 a 17) que se lee
  de vuelta exacta: `0.1` y no `0.10000000000000001`. Un `FLOAT` entero se
  escribe sin punto (`850`); al leerlo, el tipo de la columna dice que es
  `FLOAT`.
- **`Catalog::load` no escribe nada**: si no hay `metadata.txt` devuelve un
  catálogo vacío, y el primer `save()` crea el directorio. Así `--no-exec`
  nunca toca el disco.
- **Orden de escritura**: en `CREATE TABLE` y `CREATE INDEX` se escribe primero
  el archivo y después el catálogo; en `DROP` es al revés. Un crash a la mitad
  deja como mucho un archivo huérfano, nunca un catálogo que apunta a algo que
  no existe.
- **Validar antes de escribir**: `INSERT` calcula y valida todas sus filas
  (incluida la llave primaria entre filas del mismo `INSERT`) antes del primer
  `append`; `UPDATE` arma la tabla final completa antes del `rewrite`.
- **`readAt` revisa que el offset sea el inicio de una fila** (el byte anterior
  es `\n`). Un offset que no lo es da `RUN103` ("índice desactualizado") en
  lugar de leer basura.
- **`databases/escuela` trae el índice `idx_estudiantes_edad`** sobre
  `estudiantes(edad)`, al final de `crear.sql`. Así una consulta por edad
  muestra `IndexScan` sin preparar nada. Para regenerar la base después de
  cambiar `crear.sql`:
  `build/test_mutator --generar-escuela databases/escuela`. Además de los CSV,
  borra y recrea `indices/`, y `make test-mutator` compara también el `.idx`.
- **`SEM009`/`SEM011` en ejecución**: si un valor *calculado* no se puede
  convertir al tipo de la columna (el semántico solo ve literales), el
  `Mutator` lanza esos códigos con fase `ejecucion`.

## Pruebas

- `make test-storage`: ida y vuelta de cada tipo (`NULL`, cadena vacía, comas,
  comillas, saltos de línea, fechas límite, subnormales), CSV con `\r\n`,
  catálogo corrupto, `readAt`, y búsquedas por índice contra el recorrido.
- `make test-mutator`: `crear.sql` produce exactamente `databases/escuela`;
  `CREATE` → `INSERT` y `DROP` → `CREATE`; `RUN101`, `RUN102`, `RUN103` y
  `RUN104` con un caso exitoso y uno fallido que además verifica que **ningún
  archivo cambió**; `SET a = b, b = a`; y que después de `INSERT`, `UPDATE` y
  `DELETE` buscar por índice da lo mismo que filtrar todo el archivo.
- `tests/sql/casos/{ddl,dml,indices,restricciones}`: los mismos escenarios
  para el runner, con `-- archivos:` para comparar el estado final del CSV o
  del `.idx`. Los `.expected` se generaron con `make test-update` una vez
  conectada la `Session` de la Parte 3, revisando cada uno. Por ejemplo, en
  `indices/mantenimiento` el `SELECT` con `IndexScan` y el `SELECT` con
  `SeqScan` dan exactamente las mismas filas. Los casos de índices usan el
  índice que trae la base, así que además prueban que un índice cargado desde
  disco se mantiene igual que uno recién creado.

Cabeceras que entiende el runner:

```sql
-- base: vacia                     (por defecto se copia databases/escuela;
--                                   <caso>.db/ junto al .sql gana sobre ambas)
-- archivos: t.csv, indices/x.idx  (estado final; null si el archivo no existe)
```

`tests/sql/verificar_parte1.py` ahora solo recorre `lexico`, `sintactico` y
`semantico`, porque los casos de la Parte 2 no llevan `-- espera:`.
