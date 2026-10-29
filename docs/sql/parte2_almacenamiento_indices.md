# Parte 2: Almacenamiento, catálogo, modificación de datos e índices

**Proyecto:** Intérprete SQL con almacenamiento en archivos (Construcción de Compiladores, UVG 2026)
**Responsable:** _por asignar_
**Rama base:** `sql`. Tus ramas: `sql/p2-*`.

> Los acuerdos que comparten las tres partes (estructura, contratos, CLI,
> códigos de error y pruebas) están en el **Anexo** al final. Léelo primero.

## 1. De qué eres dueño

Todo lo que **toca el disco**. Eres quien hace que esto sea "con
almacenamiento en archivos" y no un intérprete en memoria:

1. El **modelo de valores y tipos** (`Value`, `SqlType`), con su conversión
   desde y hacia texto.
2. El **formato en disco**:
   - `metadata.txt` (esquemas, restricciones e índices);
   - un `.csv` por tabla;
   - un archivo por índice.
3. El **catálogo** (`Catalog`): carga y guarda los esquemas.
4. El **almacén de tablas** (`TableStore`): leer, agregar y reescribir filas.
5. La **ejecución de las sentencias que modifican**: `CREATE TABLE`,
   `DROP TABLE`, `INSERT`, `UPDATE` y `DELETE`. En la entrega 2 se suman
   `CREATE INDEX` y `DROP INDEX`.
6. Los **índices**: formato, construcción, búsqueda y mantenimiento.
7. La **base de datos de ejemplo** y el **runner de pruebas**.

Tu `value.h` y tu `catalog.h` los usan las otras dos partes desde el día 1, así
que súbelos primero.

## 2. Archivos

| Archivo | Contenido |
|---|---|
| `src/storage/value.{h,cpp}` | `Value`, `SqlType`, `Date`, `compareValues`, `parseDate` y conversión CSV. |
| `src/storage/csv.{h,cpp}` | Lector y escritor CSV estilo RFC 4180, que además informa el **offset** de cada línea. |
| `src/storage/catalog.{h,cpp}` | `Catalog`: carga y guardado de `metadata.txt`. |
| `src/storage/table_store.{h,cpp}` | `scan`, `readAt`, `append` y `rewrite`. |
| `src/storage/mutator.{h,cpp}` | Ejecución de `CREATE`/`DROP TABLE` e `INSERT`/`UPDATE`/`DELETE`. |
| `src/storage/fs_util.{h,cpp}` | Escritura atómica (archivo temporal más `rename`) y manejo de rutas. |
| `src/index/index_file.{h,cpp}` | Lectura y escritura del archivo de un índice. |
| `src/index/index_manager.{h,cpp}` | Crear, borrar, buscar y mantener índices. |
| `databases/escuela/` | Base de ejemplo. |
| `tests/sql/run_tests.py` | Runner de pruebas. |
| `tests/sql/casos/{ddl,dml,indices,restricciones}/` | Tus casos de prueba. |

## 3. Qué consumes y qué entregas

**Consumes:**
- De la Parte 1: los nodos del AST (`CreateTable`, `Insert`, `Update`, etc.),
  **ya validados** por el semántico; `Diagnostic` y `SqlError`.
- De la Parte 3:
  - `eval(expr, ctx)`, para calcular los valores del `SET` y las expresiones
    del `INSERT`;
  - `matchingRows(table, where, session)`, para encontrar las filas que
    afectan `UPDATE` y `DELETE`. **No reimplementes el `WHERE`**: así
    `UPDATE` y `DELETE` usan índices automáticamente, porque el que elige
    entre índice y recorrido es el planificador de la Parte 3.

**Entregas:**
- A todos: `value.h` y `catalog.h` (lectura).
- A la Parte 3: `TableStore::scan` y `readAt`, e `IndexManager::lookupEq` y
  `lookupRange`.
- A la Parte 1: los datos del catálogo para el panel de la IDE, mediante
  `--emit catalog`; coordinen el formato.

## 4. Formato en disco (propuesta; si lo cambias, documéntalo aquí)

El enunciado deja el formato a criterio del equipo, pero **hay que
explicarlo** en la presentación. Esta sección es esa explicación.

```
databases/escuela/
├── metadata.txt
├── estudiantes.csv
├── cursos.csv
├── inscripciones.csv
└── indices/
    └── idx_estudiantes_edad.idx
```

### 4.1 `metadata.txt`

Texto línea por línea, fácil de leer y de depurar:

```
# Generado por sqlint. No editar a mano mientras el intérprete corre.
VERSION 1

TABLE estudiantes
  COLUMN id INT NOT NULL PRIMARY KEY
  COLUMN nombre VARCHAR(50) NOT NULL
  COLUMN edad INT
  COLUMN fecha_ingreso DATE
END

TABLE inscripciones
  COLUMN estudiante_id INT NOT NULL
  COLUMN curso_id INT NOT NULL
  COLUMN nota FLOAT
END

INDEX idx_estudiantes_edad ON estudiantes (edad) FILE indices/idx_estudiantes_edad.idx
```

- Los nombres se guardan como se escribieron, pero se comparan **sin
  distinguir mayúsculas** (`Estudiantes` y `estudiantes` son la misma tabla).
- Cada cambio de esquema reescribe `metadata.txt` **de forma atómica**: se
  escribe en `metadata.txt.tmp` y luego se hace `rename`. Así un crash nunca
  deja el catálogo a medias.
- Un `metadata.txt` corrupto o un CSV faltante dan `RUN103`, con la línea del
  problema.

### 4.2 Un `.csv` por tabla

```
id,nombre,edad,fecha_ingreso
1,Ana,20,2024-01-15
2,"Pérez, Luis",17,2025-07-01
3,,19,
```

- La primera línea son los nombres de columnas, en el orden del esquema.
- Valores:
  - `INT` y `FLOAT` usan punto decimal y no llevan separador de miles;
  - `BOOLEAN` se escribe `TRUE` o `FALSE`;
  - `DATE` se escribe `YYYY-MM-DD`.
- **NULL vs cadena vacía:**
  - un campo vacío sin comillas es `NULL`;
  - `""` es la cadena vacía.

  Esta distinción importa: sin ella no hay forma de guardar `''`.
- Lleva comillas todo campo con coma, comilla, salto de línea o espacios en
  los extremos. Una comilla dentro del campo se escribe `""`.
- Codificación UTF-8 y finales de línea `\n`. **No `\r\n`**: ya tuvimos
  problemas de CRLF en Compiscript. El `.gitattributes` de la rama debe
  incluir `*.csv text eol=lf`.

### 4.3 Archivo de índice

```
# INDEX idx_estudiantes_edad ON estudiantes(edad) TYPE INT
17	58
19	94
20	31,140
```

- Una línea por **valor distinto**, ordenada por `compareValues`.
- Separado por tabulador:
  - el valor, serializado igual que en el CSV;
  - la lista de **offsets en bytes** dentro del CSV donde empieza cada fila
    con ese valor.
- Los `NULL` no se indexan. `WHERE col IS NULL` siempre hace recorrido.
- En memoria se carga como `std::map<Value, std::vector<uint64_t>, ValueLess>`,
  donde `ValueLess` usa `compareValues`. El mapa ordenado da gratis las
  búsquedas por igualdad y por rango.
- **Por qué offsets:** con el offset, `TableStore::readAt` hace `seekg` y lee
  **solo** las filas que el índice indica. Eso cumple literalmente el "en lugar
  de recorrer todos los registros" del PDF. Cargar todo el CSV y filtrar
  después no lo cumpliría.

## 5. Entrega 1

### 5.1 `Value` y tipos (semana 0 y 1; los demás dependen de esto)

- [ ] `value.h` según A.6, con stubs desde el día 1.
- [ ] `parseDate`: valida formato y fecha real, incluidos años bisiestos
  (`2024-02-29` sí, `2026-02-29` no).
- [ ] `compareValues`:
  - `INT` vs `FLOAT` se promueve a `double`;
  - las cadenas se comparan byte a byte, distinguiendo mayúsculas
    (documentarlo);
  - las fechas se comparan como `(y, m, d)`;
  - `FALSE < TRUE`.
- [ ] `fromCsvField` y `toCsvField`, simétricas: escribir y leer devuelve
  exactamente el mismo valor. Escribe una prueba unitaria de ida y vuelta
  (`make test-storage`, sin ANTLR, como el `test-symbols` de Compiscript).
- [ ] `FLOAT`: escribe con suficiente precisión para que la ida y vuelta no
  pierda nada (`%.17g`, o `std::to_chars` si el compilador lo soporta para
  `double`). Para mostrarlo en resultados, la Parte 3 decide el formato.

### 5.2 CSV y `TableStore`

- [ ] Lector que soporte comillas, comas y saltos de línea dentro de un campo,
  y que devuelva el **offset** de inicio de cada registro.
- [ ] `scan(table)`: todas las filas con su offset.
- [ ] `readAt(table, offsets)`: abre el archivo una vez, ordena los offsets,
  hace `seekg` y lee cada fila.
- [ ] `append(table, row)`: agrega al final y **devuelve el offset** (lo
  necesita el índice).
- [ ] `rewrite(table, rows)`: reescritura atómica. Devuelve los offsets nuevos.

### 5.3 Catálogo

- [ ] `Catalog::load(dir)` y `save()`, con el formato de 4.1.
- [ ] Si el directorio no existe, se crea vacío. Así "nueva base" es solo
  "directorio nuevo".
- [ ] Búsquedas sin distinguir mayúsculas.

### 5.4 `Mutator`: sentencias que modifican

Todas reciben un AST **ya validado**. No repitas las validaciones de nombres
ni de tipos del semántico, pero sí las que **solo se pueden saber con los
datos**.

- [ ] **`CREATE TABLE`:** se agrega al catálogo, se crea el CSV con su
  encabezado y se guarda el catálogo.
- [ ] **`DROP TABLE`:** se borran el CSV, **los índices de esa tabla** y su
  entrada en el catálogo.
- [ ] **`INSERT`** (varias filas):
  - las columnas que faltan quedan en `NULL`;
  - cada `VALUES` se evalúa con `eval` de la Parte 3, aunque normalmente sean
    literales;
  - se convierte al tipo de la columna: un `INT` en una columna `FLOAT` se
    guarda como `FLOAT`, y una cadena en una `DATE` se guarda como `DATE`;
  - **`PRIMARY KEY`:** si el valor ya existe, `RUN101`, y **no se inserta
    ninguna** de las filas de esa sentencia. Esto también aplica entre filas
    del mismo `INSERT`.
  - Resultado: `"N filas insertadas"`, con `affected = N`.
- [ ] **`UPDATE`:**
  - `matchingRows(table, where)` → por cada fila, se evalúan los `SET` con la
    fila **original** como contexto (semántica de SQL: `SET a = b, b = a`
    intercambia);
  - se validan `RUN102` (`NOT NULL`), `RUN104` (longitud) y `RUN101` (llave
    primaria repetida después del cambio);
  - se reescribe el CSV.
  - Si cualquier fila falla, **no se escribe nada**: se valida todo antes de
    hacer el `rewrite`.
- [ ] **`DELETE`:** `matchingRows` → se reescribe el CSV sin esas filas.
  `DELETE` sin `WHERE` vacía la tabla, pero conserva el encabezado.
- [ ] Mientras la Parte 3 no tenga `matchingRows`, usa un stub que haga `scan`
  y `eval` del `WHERE`, y reemplázalo cuando esté listo.

### 5.5 Base de ejemplo `databases/escuela`

- [ ] Las tres tablas del PDF: `estudiantes`, `cursos` e `inscripciones`, con
  **todos los tipos** (`INT`, `FLOAT`, `VARCHAR`, `BOOLEAN`, `DATE`). Por
  ejemplo, `cursos.activo BOOLEAN` e `inscripciones.nota FLOAT`.
- [ ] Unas 20 a 30 filas por tabla, pensadas para las pruebas:
  - estudiantes menores y mayores de 18 (el ejemplo del PDF);
  - nombres con coma y con comilla;
  - algunos `NULL`;
  - estudiantes sin inscripciones (para `NOT EXISTS`);
  - cursos con varias inscripciones (para `GROUP BY`).
- [ ] Un `databases/escuela/crear.sql` que la construya desde cero con
  `CREATE` e `INSERT`. Sirve como prueba y para la demo.

### 5.6 Runner de pruebas (`tests/sql/run_tests.py`)

- [ ] Recorre `tests/sql/casos/**.sql`. Para cada caso:
  1. copia la base, `databases/escuela` o el `.db` del caso, a un directorio
     temporal;
  2. corre `sqlint --json --db <tmp> caso.sql`;
  3. normaliza la salida (ordena claves y quita rutas temporales);
  4. la compara con `.expected`.
- [ ] Muestra un resumen por tema (`semantico: 40/42 ok`) y, en cada fallo, el
  diff.
- [ ] `--update` regenera los `.expected`.
- [ ] Objetivos del `Makefile`: `make test` y `make test-update`. La Parte 1
  ya los deja listados; tú los implementas.
- [ ] Opcional pero muy útil: para casos de modificación, que el `.expected`
  incluya el **estado final** del CSV afectado y no solo el mensaje.

## 6. Entrega 2: índices

### 6.1 `CREATE INDEX nombre ON tabla (columna)`

- [ ] Hacer `scan` de la tabla, construir el mapa valor → offsets y escribir el
  archivo de 4.3.
- [ ] Registrarlo en el catálogo con `INDEX ...` y guardar.
- [ ] Un índice sobre una sola columna es suficiente para el enunciado.

### 6.2 `DROP INDEX nombre`

- [ ] Borrar el archivo y la entrada del catálogo. El semántico (Parte 1) ya
  validó que exista (`SEM017`).

### 6.3 Mantenimiento (lo pide el PDF explícitamente)

| Operación | Qué pasa con los índices de la tabla |
|---|---|
| `INSERT` | **Incremental**: con el offset que devuelve `append`, se agrega a cada índice y se reescribe su archivo. |
| `UPDATE` | El `rewrite` cambia los offsets de todas las filas, así que **se reconstruyen** todos los índices de la tabla con los offsets nuevos. |
| `DELETE` | Igual que `UPDATE`: reconstrucción. |
| `DROP TABLE` | Se borran sus índices. |

- Reconstruir después de `UPDATE` y `DELETE` es simple y correcto. Su costo es
  O(n) por sentencia, lo mismo que ya cuesta reescribir el CSV. Hay que
  **explicarlo en la presentación**, porque es una decisión de diseño válida.
- Alternativa, solo si sobra tiempo: lápidas (filas marcadas como borradas) y
  compactación. No hace falta.

### 6.4 API de búsqueda para la Parte 3

- [ ] `lookupEq(idx, valor)` → offsets.
- [ ] `lookupRange(idx, lo, loIncl, hi, hiIncl)` → offsets, con
  `lower_bound`/`upper_bound` sobre el mapa. Cubre `<`, `<=`, `>` y `>=`.
- [ ] Caché del índice en memoria durante una `Session`, invalidada cuando lo
  modificas.
- [ ] Las conversiones de tipo son tu responsabilidad. Por ejemplo, buscar
  `edad = 18.0` en un índice `INT` debe encontrar el 18.

### 6.5 Opcional

- [ ] Índice automático sobre la `PRIMARY KEY`, para que la verificación de
  `RUN101` no recorra toda la tabla. Avísale a la Parte 1, porque ese índice no
  debe contar como "duplicado" (`SEM016`) si alguien crea otro.

## 7. Pruebas que te tocan

- [ ] Ida y vuelta de valores: cada tipo, `NULL`, cadena vacía, comas,
  comillas, saltos de línea y fechas límite.
- [ ] `CREATE` → `INSERT` → `SELECT` y `DROP` → `CREATE` de nuevo con el mismo
  nombre.
- [ ] `RUN101`, `RUN102`, `RUN103` y `RUN104`: un caso exitoso y uno fallido
  cada uno. El fallido verifica además que **el archivo no cambió**.
- [ ] Índices:
  - el plan dice `IndexScan` cuando corresponde (con la Parte 3);
  - después de `INSERT`, `UPDATE` y `DELETE`, un `SELECT` por índice devuelve
    lo mismo que uno sin índice. Es **la prueba más importante**: compara los
    dos resultados en el mismo caso;
  - `DROP TABLE` borra el archivo `.idx`.

## 8. Cuándo está terminado

- Todo lo que el intérprete sabe está en archivos de texto legibles. Si borras
  el binario y lo vuelves a ejecutar, la base sigue igual.
- Ninguna sentencia que falle deja la base modificada a medias.
- Los índices se guardan en archivos, se usan en el `WHERE` (con la Parte 3) y
  quedan correctos después de cualquier `INSERT`, `UPDATE` o `DELETE`.
- El formato está documentado (sección 4) para la presentación.

## 9. Riesgos y consejos

- **Offsets y finales de línea:** si un CSV llega con `\r\n` (porque alguien
  lo editó en Windows), los offsets siguen siendo válidos, pero el parser debe
  tolerar el `\r`. Nunca escribas `\r\n`.
- **Cachés desactualizados:** si guardas el catálogo o el índice en memoria,
  invalídalos en **cada** escritura. Es la fuente número uno de bugs de "el
  índice devuelve filas que ya no existen".
- **Archivos abiertos durante `rename`:** en WSL sobre `/mnt/c` (NTFS),
  renombrar un archivo que sigue abierto puede fallar. Cierra siempre los
  `ifstream` antes del `rename`.
- La IDE lee el catálogo con `--emit catalog`. No dejes que nadie lea
  `metadata.txt` directamente desde Python.

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
