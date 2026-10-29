# Parte 3: Ejecución de consultas (evaluador, SELECT, JOIN, agregados, subconsultas y plan)

**Proyecto:** Intérprete SQL con almacenamiento en archivos (Construcción de Compiladores, UVG 2026)
**Responsable:** _por asignar_
**Rama base:** `sql`. Tus ramas: `sql/p3-*`.

> Los acuerdos que comparten las tres partes (estructura, contratos, CLI,
> códigos de error y pruebas) están en el **Anexo** al final. Léelo primero.

## 1. De qué eres dueño

La etapa **"Interpretación"** del diagrama del PDF: tomar un AST ya validado y
producir resultados.

1. La **sesión** (`Session`): recorre las sentencias, llama al semántico,
   despacha cada sentencia a quien la ejecuta y junta los resultados. Es el
   pegamento entre las tres partes.
2. El **evaluador de expresiones**: aritmética, comparaciones, lógica de tres
   valores con `NULL`, fechas y subconsultas.
3. **`SELECT`**: proyección, `WHERE`, `ORDER BY`, alias y `*`.
4. **`INNER JOIN`**, **agregados**, **`GROUP BY`** y **`HAVING`**.
5. **Subconsultas**: `IN`, `NOT IN`, `EXISTS`, `NOT EXISTS` y escalares,
   correlacionadas o no.
6. El **planificador**: decide si un `WHERE` usa índice o recorre la tabla, y
   reporta esa decisión en el campo `plan`.
7. La **salida de resultados**: el JSON de A.7 y las tablas de texto para la
   consola.

Tu evaluador lo usa la Parte 2 en `INSERT`, `UPDATE` y `DELETE`, así que debe
estar listo en la **semana 1**, aunque al inicio solo soporte literales y
columnas.

## 2. Archivos

| Archivo | Contenido |
|---|---|
| `src/exec/session.{h,cpp}` | `Session::run`: ciclo de sentencias, despacho y política de errores. |
| `src/exec/eval_context.h` | Filas actuales por fuente, más un puntero al contexto externo. |
| `src/exec/evaluator.{h,cpp}` | `eval`, `isTrue`, operadores y lógica de tres valores. |
| `src/exec/select.{h,cpp}` | `SelectExecutor`: el pipeline completo de un `SELECT`. |
| `src/exec/join.{h,cpp}` | `INNER JOIN` (nested loop; opcionalmente por índice). |
| `src/exec/aggregate.{h,cpp}` | Agrupación y acumuladores `COUNT`, `SUM`, `AVG`, `MIN` y `MAX`. |
| `src/exec/subquery.{h,cpp}` | Evaluación de subconsultas, con caché para las no correlacionadas. |
| `src/exec/planner.{h,cpp}` | `matchingRows` y la elección entre `SeqScan` e `IndexScan`. |
| `src/exec/result_json.{h,cpp}` | Serialización de `StatementResult` a JSON y a tabla de texto. |
| `tests/sql/casos/{select,join,agregados,subconsultas,plan}/` | Tus casos de prueba. |

## 3. Qué consumes y qué entregas

**Consumes:**
- De la Parte 1:
  - el AST **anotado**. Cada `ColumnRef` trae `binding {depth, source, column}`
    y cada `Expr` trae su `type`. **No vuelvas a resolver nombres**: si algo no
    está anotado, es un bug del semántico, y hay que reportárselo a la Parte 1;
  - `analyze(stmt, catalog)`, que llamas desde la `Session` antes de ejecutar
    cada sentencia.
- De la Parte 2:
  - `Value`, `compareValues` y `Catalog`;
  - `TableStore::scan` y `readAt`;
  - `IndexManager::lookupEq` y `lookupRange`;
  - `Mutator`, para despacharle las sentencias que modifican.

**Entregas:**
- A la Parte 2: `eval`, `isTrue` y `matchingRows`.
- A la Parte 1: `Session::run` (lo llama `main.cpp`), y el JSON de resultados
  que consume la IDE.

## 4. Diseño del ejecutor

Modelo simple y fácil de explicar: **materializar cada paso como un vector de
filas**. Las bases del proyecto son pequeñas, así que no hace falta un modelo
de iteradores (Volcano).

```
FROM / JOIN           ->  vector<Tupla>
WHERE                 ->  vector<Tupla>
GROUP BY + agregados  ->  vector<Grupo>
HAVING                ->  vector<Grupo>
SELECT (proyección)   ->  vector<Row>      (+ la clave de orden si hace falta)
ORDER BY              ->  vector<Row>
```

Una **`Tupla`** es una fila combinada: `std::vector<const Row*>`, con una
entrada por fuente del `FROM`/`JOIN`, en el mismo orden que el
`binding.source` del semántico.

Así una `ColumnRef` se evalúa en O(1):

```
tupla[binding.source]->at(binding.column)
```

Si `binding.depth > 0`, se sube por `ctx.outer` esa cantidad de niveles.

## 5. Entrega 1

### 5.1 Evaluador (semana 1, primero los literales y las columnas)

- [ ] **Literales** de los 5 tipos y `NULL`.
- [ ] **`ColumnRef`** por `binding`, incluido `depth > 0`. El `outer` queda
  listo aunque las subconsultas lleguen en la entrega 2.
- [ ] **Aritmética** `+ - * / %`:
  - `INT` con `INT` da `INT`; si alguno es `FLOAT`, da `FLOAT`;
  - `/` entre enteros es división entera (lo acordado con la Parte 1);
  - división o módulo entre cero da `RUN202`;
  - cualquier operando `NULL` da `NULL`.
- [ ] **Comparaciones:** con `compareValues` de la Parte 2. Si algún lado es
  `NULL`, el resultado es `NULL` (no `false`).
- [ ] **Lógica de tres valores:**

  | `AND` | T | F | N |
  |---|---|---|---|
  | **T** | T | F | N |
  | **F** | F | F | F |
  | **N** | N | F | N |

  | `OR` | T | F | N |
  |---|---|---|---|
  | **T** | T | T | T |
  | **F** | T | F | N |
  | **N** | T | N | N |

  `NOT NULL` es `NULL`. `isTrue(v)` es verdadero **solo** si `v` es `TRUE`; un
  `WHERE` que evalúa a `NULL` descarta la fila.
- [ ] **`IS NULL` / `IS NOT NULL`:** nunca dan `NULL`.
- [ ] **`IN (lista)`:**
  - si encuentra el valor: `TRUE`;
  - si no lo encuentra y en la lista hay un `NULL`: `NULL`;
  - si no: `FALSE`.

  `NOT IN` es la negación de esto. Por eso `x NOT IN (1, NULL)` nunca es
  `TRUE`; es una trampa clásica y conviene probarla.
- [ ] **Coerción de cadena a fecha:** si el semántico anotó que una cadena se
  compara con una `DATE`, conviértela aquí con `parseDate`. Es mejor que el
  semántico ya la deje convertida en el AST; acuérdenlo con la Parte 1.

### 5.2 Sesión (`Session::run`)

- [ ] Carga el `Catalog` de `--db`.
- [ ] Por cada sentencia:
  1. `analyze(stmt, catalog)`. Si hay errores, se agregan y **se detiene**;
  2. si no, se despacha según la tabla de A.5;
  3. se captura `SqlError` (`RUN*`), se agrega como diagnóstico y **se
     detiene**;
  4. si una sentencia cambió el esquema, se recarga el `Catalog`.
- [ ] Con `--no-exec`, solo se corre el semántico. Ojo: sin ejecutar, un
  `CREATE TABLE` no llega al catálogo y el `INSERT` siguiente daría `SEM001`.
  Para el modo `--no-exec`, simula el efecto de los DDL en una **copia en
  memoria** del catálogo (coordínalo con la Parte 2).
- [ ] Llena `StatementResult`: `kind`, `line`, `status`, columnas, filas,
  `affected`, `plan` y `message`.

### 5.3 `SELECT` sobre una tabla

- [ ] **`FROM`:** `matchingRows(table, where)` ya devuelve filtrado, ya sea por
  índice o por recorrido completo con el `WHERE` evaluado.
- [ ] **Proyección:**
  - `*` expande todas las columnas, en el orden del esquema;
  - `t.*` expande solo las de `t`;
  - expresiones con alias.
  - Nombre de columna del resultado: el alias si lo hay; si es una columna
    simple, su nombre; si no, el texto de la expresión (`edad + 1`).
  - Tipo: el que anotó el semántico.
- [ ] **`ORDER BY`:**
  - varias claves, cada una `ASC` o `DESC`;
  - `std::stable_sort`;
  - los `NULL` van **al final en `ASC` y al inicio en `DESC`**, es decir, se
    tratan como el valor más grande (documentarlo);
  - se puede ordenar por un alias de la selección o por una columna que no
    está en la selección. En ese caso, calcula la clave antes de proyectar.
- [ ] **Salida:**
  - JSON (A.7);
  - tabla de texto alineada para la consola;
  - un `FLOAT` se muestra con hasta 6 decimales sin ceros sobrantes (`3.5`,
    no `3.500000`).

### 5.4 `matchingRows` (planificador, versión de la entrega 1)

- [ ] En la entrega 1 siempre es `SeqScan`: `scan` y luego `isTrue(eval(where))`.
  Devuelve `plan = "SeqScan en <tabla>"`.
- [ ] Mantén la firma de A.6. En la entrega 2 solo cambia el interior, y la
  Parte 2 no tiene que tocar nada.

## 6. Entrega 2

### 6.1 `INNER JOIN`

- [ ] **Nested loop:** por cada tupla acumulada y cada fila de la tabla
  siguiente, se evalúa `ON` y, si es verdadero, se extiende la tupla. Soporta
  N joins encadenados.
- [ ] Empuja el `WHERE` cuando se pueda. El caso mínimo: los términos del
  `WHERE` que solo usan la primera tabla se aplican en `matchingRows` antes
  del join. Así el índice también sirve en consultas con `JOIN`.
- [ ] Opcional (luce en la presentación): si `ON` es `a.x = b.y` y `b.y` tiene
  índice, hacer `lookupEq` por cada fila de `a` en lugar de recorrer `b`.
  Plan: `IndexNestedLoop`.

### 6.2 Agregados, `GROUP BY` y `HAVING`

- [ ] Clave de grupo: el vector de valores de las expresiones del `GROUP BY`.
  Usa `std::map` con un comparador basado en `compareValues`. Los `NULL`
  forman **un** grupo propio.
- [ ] Acumuladores por grupo:

  | Función | Regla |
  |---|---|
  | `COUNT(*)` | Cuenta filas. |
  | `COUNT(expr)` | Cuenta los valores no `NULL`. |
  | `SUM` | Suma los no `NULL`. Si todos son `NULL` (o no hay filas), da `NULL`. Es `INT` si la columna es `INT`. |
  | `AVG` | `FLOAT`. Mismo manejo de `NULL` que `SUM`. |
  | `MIN`, `MAX` | Con `compareValues`, ignorando los `NULL`. |

- [ ] **Agregados sin `GROUP BY`:** toda la tabla es **un** grupo. Sobre una
  tabla vacía, `COUNT` da `0` y los demás dan `NULL`, y el resultado tiene
  **una fila**, no cero.
- [ ] **`HAVING`:** se evalúa sobre el grupo. Ahí los agregados se resuelven
  con los acumuladores del grupo, y las columnas del `GROUP BY` con su valor de
  grupo. El semántico ya garantizó (`SEM021`) que no aparece nada más.
- [ ] Evaluación de expresiones con agregados, por ejemplo `SUM(nota) / COUNT(*)`:
  1. **antes** de agrupar, junta todos los nodos `Aggregate` de `SELECT`,
     `HAVING` y `ORDER BY`;
  2. calcula un acumulador por cada uno y por grupo;
  3. al evaluar, el nodo `Aggregate` devuelve el valor ya calculado. Puedes
     pasarlo en el `EvalContext` como un mapa `nodo → Value`.

### 6.3 Subconsultas

- [ ] **No correlacionadas** (`isCorrelated == false`): se ejecutan **una sola
  vez** y se guardan en caché durante la sentencia.
- [ ] **Correlacionadas:** se re-ejecutan por cada tupla externa, con
  `EvalContext.outer` apuntando a la tupla externa. Por ejemplo:

  ```sql
  SELECT nombre FROM estudiantes e
  WHERE NOT EXISTS (SELECT 1 FROM inscripciones i
                    WHERE i.estudiante_id = e.id);
  ```

- [ ] **`IN` / `NOT IN` (subconsulta):** misma semántica de `NULL` que con
  listas (5.1).
- [ ] **`EXISTS` / `NOT EXISTS`:** verdadero si hay al menos una fila. Puedes
  cortar en cuanto aparece la primera. Nunca da `NULL`.
- [ ] **Escalar:**
  - 0 filas: `NULL`;
  - 1 fila: su único valor;
  - más de 1 fila: **`RUN201`**, con la línea y columna de la subconsulta. Es
    la regla del PDF *"subconsulta escalar que retorna más de un registro"*;
    la parte de "más de una columna" la cubre la Parte 1 con `SEM018`.
- [ ] El plan de cada subconsulta se anida en el plan de la sentencia, por
  ejemplo:

  ```
  SeqScan en estudiantes
    └ Subconsulta (correlacionada): IndexScan idx_insc_est en inscripciones
  ```

### 6.4 Planificador con índices (lo pide el PDF explícitamente)

> *"Al evaluar condiciones de WHERE sobre una columna indexada, el intérprete
> deberá utilizar el índice en lugar de recorrer todos los registros."*

- [ ] Partir el `WHERE` en sus términos unidos por `AND` (conjunciones).
- [ ] Buscar el **primer término indexable**, de la forma `col op constante`
  o `constante op col`:
  - `col` es una columna de esta tabla (`depth == 0`) con un índice
    (`catalog.indexesOn`);
  - `op` es uno de `=`, `<`, `<=`, `>` o `>=`;
  - "constante" es un literal, o una expresión sin columnas de esta tabla. Por
    ejemplo, una columna de la consulta externa en una subconsulta
    correlacionada es constante para la consulta interna, y una subconsulta
    escalar no correlacionada también.
  - Opcional: combinar `col >= a AND col <= b` en un solo rango.
- [ ] Si se encuentra:
  1. `lookupEq` o `lookupRange` dan los offsets;
  2. `readAt` lee solo esas filas;
  3. se evalúa **el `WHERE` completo** sobre ellas, porque los otros términos
     siguen aplicando.

  Plan: `IndexScan <índice> (<col> <op> <valor>) en <tabla>`.
- [ ] Si no se encuentra (no hay índice, hay `OR` en la raíz, `IS NULL`, o
  `col` está dentro de una expresión como `edad + 1 = 20`): `SeqScan`.
- [ ] `matchingRows` lo usa siempre. Así **`UPDATE` y `DELETE` de la Parte 2
  también usan el índice**, sin trabajo extra para ellos.

## 7. Pruebas que te tocan

- [ ] **Evaluador:** las tablas de verdad de 5.1 completas, aritmética con
  `NULL`, `RUN202`, y `NOT IN` con `NULL`.
- [ ] **`SELECT`:** el ejemplo de la página 4 del PDF
  (`SELECT nombre FROM estudiantes WHERE edad >= 18;`), `*`, alias,
  `ORDER BY` de varias claves con `NULL`.
- [ ] **`JOIN`:** dos y tres tablas, `JOIN` sin coincidencias (resultado
  vacío), y `t.*`.
- [ ] **Agregados:** cada función con y sin `GROUP BY`, tabla vacía, `HAVING`
  con un agregado que no está en el `SELECT`.
- [ ] **Subconsultas:** el ejemplo de la página 3 del PDF; `NOT EXISTS`
  correlacionado; escalar con 0, 1 y 2 o más filas (este último da `RUN201`).
- [ ] **Plan:** cada caso con índice verifica el string `plan`, y lo compara
  con el mismo `SELECT` sin índice. El resultado tiene que ser idéntico.

## 8. Cuándo está terminado

- Todo el "SQL Requerido" del PDF se ejecuta y da resultados correctos,
  incluidos los casos con `NULL`.
- El campo `plan` demuestra cuándo se usa un índice, y la IDE lo muestra.
- `RUN201` y `RUN202` tienen su caso exitoso y su caso fallido.
- La `Session` detiene el script en el primer error y reporta la ubicación
  exacta.

## 9. Riesgos y consejos

- **Eres el integrador.** Cuando algo no funcione de punta a punta, el bug va a
  aparecer en tu código aunque venga de otra parte. Ten pruebas pequeñas por
  capa para poder decir rápido "esto viene del semántico" o "esto viene del
  CSV".
- **No re-resuelvas nombres.** Si te descubres buscando una columna por nombre
  dentro del ejecutor, el `binding` del semántico está mal: pide el arreglo a
  la Parte 1.
- **Agregados en `ORDER BY`** (`ORDER BY COUNT(*) DESC`): calcula la clave de
  orden **sobre el grupo**, antes de proyectar.
- **Tiempo:** `JOIN` y agregados son lo más rápido de la entrega 2. Las
  subconsultas correlacionadas y el planificador son lo más delicado: empiézalos
  pronto.

---

## Anexo: acuerdos comunes a las tres partes

> Este anexo es **idéntico** en los tres documentos. Si se cambia un acuerdo,
> hay que cambiarlo en los tres.

### A.1 Organización del trabajo

- **Rama del proyecto: `sql`**, creada desde `main`. Nada de este proyecto va a
  `main`, que sigue siendo de Compiscript.
- Cada integrante trabaja en **ramas propias que salen de `sql`**, por ejemplo
  `sql/p1-gramatica`, `sql/p2-indices` o `sql/p3-join`. Se integran a `sql` con
  un pull request que revisa otro integrante.
- **Commits individuales.** El curso revisa quién hizo qué, así que:
  - no se hacen commits compartidos;
  - nadie sube código de otro con su propio usuario.
- **Antes del primer commit en cada computadora**, revisa la identidad de git:

  ```bash
  git config user.name
  ```

  ```bash
  git config user.email
  ```
- **Stack:** C++17 con ANTLR4 (target C++) para el intérprete, y Python con
  Tkinter para la IDE. Se reutilizan `tools/setup.sh` y la estructura del
  `Makefile` de Compiscript. Se compila y se corre en WSL/Linux.
- **Prohibido por el enunciado:** usar SQLite, MySQL, PostgreSQL o cualquier
  otro DBMS para almacenar o para ejecutar.

### A.2 Las dos entregas (propuesta, falta confirmarla con el catedrático)

El PDF enumera 8 hitos y nosotros tenemos 2 entregas. Propuesta de corte:

| Entrega | Hitos del PDF | Contenido |
|---|---|---|
| **1** | 1 a 5 | Lexer y parser con la gramática **completa**; AST; análisis semántico y errores; `CREATE TABLE`, `DROP TABLE`, `INSERT`; `SELECT … FROM … WHERE … ORDER BY` sobre una tabla; `UPDATE`, `DELETE`; IDE con todas las vistas. |
| **2** | 6 a 8 | `INNER JOIN`; `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`; `GROUP BY` y `HAVING`; `CREATE INDEX` y `DROP INDEX`, con uso del índice en `WHERE` y mantenimiento; subconsultas (`IN`, `NOT IN`, `EXISTS`, `NOT EXISTS`, escalares); integración, batería final de pruebas y pulido de la IDE. |

Fechas: **pendiente**, las llena el equipo.

### A.3 Reparto en una línea

| Parte | Responsable | Dueño de |
|---|---|---|
| **1** | _por asignar_ | Gramática, parser, AST, análisis semántico, diagnósticos, CLI e IDE. |
| **2** | _por asignar_ | Tipos y valores, archivos CSV, catálogo (`metadata.txt`), `CREATE`/`DROP TABLE`, `INSERT`/`UPDATE`/`DELETE`, índices, base de datos de ejemplo y runner de pruebas. |
| **3** | _por asignar_ | Sesión (orquesta todo), evaluador de expresiones, `SELECT`, `ORDER BY`, `JOIN`, agregados, `GROUP BY`/`HAVING`, subconsultas, elección de índice (plan) y salida de resultados. |

### A.4 Estructura de directorios en la rama `sql`

```
grammar/SQL.g4                    Parte 1
src/main.cpp                      Parte 1 (CLI)
src/frontend/                     Parte 1  parser_driver, error_listener, ast_builder,
                                           token_dump, parse_tree_dot
src/ast/                          Parte 1  nodes.h, printer, ast_dot
src/diagnostics/                  Parte 1  diagnostic.h, codes.h, reporter
src/semantic/                     Parte 1  analyzer, scope, type_rules
src/storage/                      Parte 2  value, csv, catalog, table_store, mutator
src/index/                        Parte 2  index_file, index_manager
src/exec/                         Parte 3  session, evaluator, select, join, aggregate,
                                           subquery, planner, result_json
ide/sql_ide.py                    Parte 1
databases/escuela/                Parte 2  base de ejemplo (estudiantes, cursos, inscripciones)
tests/sql/run_tests.py            Parte 2  runner
tests/sql/casos/<tema>/           todos    cada quien los casos de sus reglas
docs/sql/                         todos
```

El código de Compiscript (`src/semantic`, `src/codegen`, `src/ir`, etc.) **se
borra en la rama `sql`**: sigue en `main`, y en esta rama solo causaría
confusión.

### A.5 Flujo de ejecución de un script

```
script.sql
  │  1. léxico + sintáctico de TODO el script
  │     (si hay al menos un error, no se ejecuta nada)
  ▼
lista de sentencias (AST)
  │  2. para cada sentencia, en orden:
  │       a. análisis semántico contra el catálogo ACTUAL   (Parte 1)
  │       b. ejecución                                      (Parte 2 o 3)
  │       c. se agrega el resultado a la salida
  │     El primer error semántico o de ejecución detiene el script.
  │     Lo que ya se ejecutó queda guardado en disco (no hay transacciones).
  ▼
salida: diagnósticos + resultados
```

El semántico se hace **sentencia por sentencia y no todo de una vez** porque un
script puede hacer `CREATE TABLE t (...)` y en la línea siguiente
`INSERT INTO t ...`. Cuando se analiza el `INSERT`, `t` ya tiene que estar en
el catálogo.

Despacho por tipo de sentencia (lo hace la `Session` de la Parte 3):

| Sentencia | Ejecuta |
|---|---|
| `CREATE TABLE`, `DROP TABLE`, `INSERT`, `UPDATE`, `DELETE` | Parte 2 (`Mutator`) |
| `CREATE INDEX`, `DROP INDEX` | Parte 2 (`IndexManager`) |
| `SELECT` | Parte 3 (`SelectExecutor`) |

### A.6 Contratos entre partes (headers)

En la **semana 0**, cada dueño sube su header con las firmas y stubs que
compilen; los stubs devuelven un error "no implementado". Así nadie se bloquea
esperando a otro. El `Makefile` toma todos los `.cpp` con `wildcard`, así que
un archivo nuevo entra solo.

```cpp
// ── src/diagnostics/diagnostic.h ─────────────────────── Parte 1
enum class Phase { Lexico, Sintactico, Semantico, Ejecucion };
struct SourceLoc { int line = 0; int col = 0; };     // 1-based
struct Diagnostic {
    Phase phase;
    std::string code;        // "SEM002", "RUN201", ...
    SourceLoc loc;
    std::string message;     // en español, menciona tabla/columna concretas
    int length = 1;          // caracteres a subrayar en la IDE
};
class SqlError : public std::exception { public: Diagnostic diag; /* ... */ };
// Los errores de ejecución (Partes 2 y 3) se lanzan como SqlError.

// ── src/storage/value.h ──────────────────────────────── Parte 2
enum class TypeKind { Null, Int, Float, Varchar, Boolean, Date };
struct SqlType { TypeKind kind; int length = 0; };   // length solo para VARCHAR(n)
struct Date { int y, m, d; };
struct Value {
    TypeKind kind = TypeKind::Null;
    std::variant<std::monostate, int64_t, double, std::string, bool, Date> v;
    bool isNull() const;
};
int  compareValues(const Value& a, const Value& b); // -1/0/1. INT vs FLOAT se promueve.
                                                    // No acepta NULL (quien llama lo decide).
std::string toCsvField(const Value&);               // serializa
Value fromCsvField(const std::string&, const SqlType&); // parsea, lanza SqlError si no cuadra
bool  parseDate(const std::string&, Date&);         // valida YYYY-MM-DD real (no 2026-02-30)
std::string typeName(const SqlType&);               // "VARCHAR(50)"

// ── src/storage/catalog.h ────────────────────────────── Parte 2
struct ColumnDef { std::string name; SqlType type; bool notNull = false; bool primaryKey = false; };
struct TableDef  { std::string name; std::vector<ColumnDef> columns;
                   int findColumn(const std::string&) const; };  // -1 si no existe
struct IndexDef  { std::string name, table, column, file; };
class Catalog {
public:
    static Catalog load(const std::string& dbDir);
    const TableDef* findTable(const std::string&) const;   // nombres case-insensitive
    const IndexDef* findIndex(const std::string&) const;
    std::vector<const IndexDef*> indexesOn(const std::string& table, const std::string& column) const;
    std::vector<const TableDef*> tables() const;
    // mutadores: solo los usa la Parte 2
};

// ── src/storage/table_store.h ────────────────────────── Parte 2
using Row = std::vector<Value>;
struct StoredRow { uint64_t offset; Row values; };   // offset = byte donde empieza la línea en el CSV
class TableStore {
public:
    std::vector<StoredRow> scan(const TableDef&);                           // recorrido completo
    std::vector<StoredRow> readAt(const TableDef&, const std::vector<uint64_t>& offsets); // acceso directo
};

// ── src/index/index_manager.h ────────────────────────── Parte 2 (entrega 2)
class IndexManager {
public:
    std::vector<uint64_t> lookupEq(const IndexDef&, const Value&);
    std::vector<uint64_t> lookupRange(const IndexDef&,
        const std::optional<Value>& lo, bool loIncl,
        const std::optional<Value>& hi, bool hiIncl);
};

// ── src/ast/nodes.h ──────────────────────────────────── Parte 1
// Statement: CreateTable, DropTable, CreateIndex, DropIndex, Insert, Update, Delete, Select
// Expr: Literal, ColumnRef, Unary, Binary, IsNull, InList, InSubquery, Exists,
//       ScalarSubquery, Aggregate, Star
// Todo nodo tiene SourceLoc. El semántico ANOTA el AST:
//   Expr::type          -> SqlType resultante
//   ColumnRef::binding  -> { int depth; int source; int column; }
//       depth  = 0 si es de la consulta actual, 1 si es de la consulta externa, ...
//       source = índice de la tabla dentro del FROM/JOIN de esa consulta
//       column = índice de la columna dentro de esa tabla
//   Select::isCorrelated -> true si alguna ColumnRef interna tiene depth > 0
// La Parte 3 ejecuta usando estas anotaciones y NO vuelve a resolver nombres.

// ── src/semantic/analyzer.h ──────────────────────────── Parte 1
std::vector<Diagnostic> analyze(ast::Statement&, const Catalog&);

// ── src/exec/evaluator.h ─────────────────────────────── Parte 3 (listo en la semana 1)
struct EvalContext { /* fila actual por fuente + contexto externo (subconsultas) */ };
Value eval(const ast::Expr&, const EvalContext&);
bool  isTrue(const Value&);                          // lógica de tres valores: NULL -> false

// ── src/exec/planner.h ───────────────────────────────── Parte 3
// Devuelve las filas de UNA tabla que cumplen el WHERE, usando un índice si se
// puede. La Parte 2 lo usa en UPDATE y DELETE, así esas sentencias también
// aprovechan los índices.
struct ScanResult { std::vector<StoredRow> rows; std::string plan; };
ScanResult matchingRows(const TableDef&, const ast::Expr* where, Session&);

// ── src/exec/session.h ───────────────────────────────── Parte 3
struct StatementResult { /* kind, columnas, filas, affected, plan, mensaje */ };
class Session { public: Session(std::string dbDir); RunOutput run(const ParsedScript&); };
```

### A.7 Línea de comandos (contrato con la IDE)

```
sqlint --db <directorio> [opciones] <archivo.sql>

  (sin opciones)        ejecuta y muestra resultados como tablas de texto
  --json                toda la salida como un único JSON (lo que usa la IDE)
  --emit tokens         lista de tokens: tipo, lexema, línea, columna
  --emit parse-tree     árbol sintáctico de ANTLR en formato .dot
  --emit ast            AST como texto indentado
  --emit ast-dot        AST en formato .dot
  --emit catalog        catálogo (tablas, columnas, tipos, índices) en JSON, para la IDE (entrega 2)
  --no-exec             solo léxico, sintáctico y semántico (no toca disco)
```

Formato `--json`:

```json
{
  "diagnostics": [
    {"phase": "semantico", "code": "SEM002", "line": 3, "col": 8,
     "message": "La columna 'edad2' no existe en la tabla 'estudiantes'"}
  ],
  "statements": [
    {"index": 0, "line": 1, "kind": "SELECT", "status": "ok",
     "columns": [{"name": "nombre", "type": "VARCHAR(50)"}],
     "rows": [["Ana"], ["Luis"]],
     "affected": null,
     "plan": "IndexScan idx_edad (edad >= 18) en estudiantes",
     "message": "2 filas"}
  ]
}
```

En el JSON, los valores van así:
- `INT` y `FLOAT` como números;
- `BOOLEAN` como `true`/`false`;
- `DATE` y `VARCHAR` como texto;
- `NULL` como `null`.

### A.8 Códigos de diagnóstico

La Parte 1 es dueña de `codes.h`. Las otras partes piden su código y la
Parte 1 lo agrega; los rangos ya están reservados para que no haya choques.

| Código | Fase | Regla | Detecta |
|---|---|---|---|
| `LEX001` | léxico | Carácter no reconocido | P1 |
| `LEX002` | léxico | Cadena sin cerrar | P1 |
| `SYN001` | sintáctico | Error de sintaxis (mensaje de ANTLR traducido) | P1 |
| `SEM001` | semántico | Tabla inexistente | P1 |
| `SEM002` | semántico | Columna inexistente | P1 |
| `SEM003` | semántico | Referencia ambigua (columna sin calificar presente en 2 o más tablas del `FROM`) | P1 |
| `SEM004` | semántico | Calificador inválido (`x.col` con `x` que no está en el `FROM`) | P1 |
| `SEM005` | semántico | Alias o tabla repetida en el `FROM` | P1 |
| `SEM006` | semántico | Tipos incompatibles en una comparación | P1 |
| `SEM007` | semántico | Tipos incompatibles en aritmética o lógica (incluye `WHERE` no booleano) | P1 |
| `SEM008` | semántico | `INSERT`: cantidad de valores distinta a la de columnas | P1 |
| `SEM009` | semántico | `INSERT`/`UPDATE`: valor de tipo incompatible con la columna | P1 |
| `SEM010` | semántico | Literal `VARCHAR` más largo que `n` | P1 |
| `SEM011` | semántico | Fecha inválida (formato o fecha imposible) | P1 |
| `SEM012` | semántico | `NULL` literal en columna `NOT NULL` | P1 |
| `SEM013` | semántico | `CREATE TABLE` de una tabla que ya existe | P1 |
| `SEM014` | semántico | `CREATE TABLE` mal formado: columna repetida, más de una `PRIMARY KEY`, `VARCHAR(0)` | P1 |
| `SEM015` | semántico | Columna repetida en la lista del `INSERT` o en el `SET` | P1 |
| `SEM016` | semántico | Índice duplicado: mismo nombre, o ya hay un índice sobre esa columna | P1 |
| `SEM017` | semántico | Índice inexistente (`DROP INDEX`) | P1 |
| `SEM018` | semántico | Subconsulta escalar con más de una columna | P1 |
| `SEM019` | semántico | Subconsulta de `IN` con más de una columna | P1 |
| `SEM020` | semántico | Agregado en `WHERE`, o agregados anidados | P1 |
| `SEM021` | semántico | Columna no agrupada en `SELECT`, `HAVING` u `ORDER BY` cuando hay `GROUP BY` o agregados | P1 |
| `SEM022` | semántico | `SUM`/`AVG` sobre un tipo no numérico | P1 |
| `SEM023` | semántico | Construcción que todavía no se soporta en la entrega actual (por ejemplo, `JOIN` en la entrega 1) | P1 |
| `RUN101` | ejecución | Llave primaria duplicada | P2 |
| `RUN102` | ejecución | `NOT NULL` violado por una expresión en `UPDATE` | P2 |
| `RUN103` | ejecución | Archivo de la base faltante o corrupto | P2 |
| `RUN104` | ejecución | Valor calculado en `UPDATE` excede el `VARCHAR(n)` | P2 |
| `RUN201` | ejecución | Subconsulta escalar devolvió más de una fila | P3 |
| `RUN202` | ejecución | División entre cero | P3 |

El caso "subconsulta escalar con más de un **registro**" no se puede saber
antes de ejecutar. Por eso hay dos códigos: `SEM018` (columnas, se detecta en
el semántico) y `RUN201` (filas, se detecta al ejecutar).

### A.9 Pruebas

```
tests/sql/casos/<tema>/<nombre>.sql         el caso
tests/sql/casos/<tema>/<nombre>.expected    salida esperada (JSON normalizado)
tests/sql/casos/<tema>/<nombre>.db          (opcional) base inicial; si no hay, se usa databases/escuela
```

- El runner (Parte 2) copia la base a un directorio temporal, corre
  `sqlint --json` y compara. Así ninguna prueba ensucia la base de ejemplo.
- `make test` corre todo.
- `make test-update` regenera los `.expected`. Hay que **revisar el diff**
  antes de hacer commit.
- Regla del enunciado: **cada regla tiene al menos un caso exitoso y uno
  fallido**. El caso fallido verifica **el código exacto**, no solo que falle.
- Cada quien escribe los casos de las reglas que detecta su parte (tabla A.8).

### A.10 Semana 0 (antes de programar)

1. Leer el PDF y estos tres documentos completos.
2. Confirmar el corte de entregas (A.2) y asignar las partes (A.3).
3. **Parte 1** crea la rama `sql` con el esqueleto: borra lo de Compiscript,
   adapta `Makefile` y `tools/setup.sh`, y deja `SQL.g4` con al menos
   `CREATE TABLE` y `SELECT`.
4. Cada parte sube los headers de su sección de A.6, con stubs que compilen.
5. **Parte 2** sube `databases/escuela` y el runner, aunque al principio todas
   las pruebas fallen.
6. Desde ahí, cada quien trabaja en su rama y abre pull requests a `sql`.
