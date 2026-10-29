# Parte 1: Gramática, AST, análisis semántico e IDE

**Proyecto:** Intérprete SQL con almacenamiento en archivos (Construcción de Compiladores, UVG 2026)
**Responsable:** _por asignar_
**Rama base:** `sql`. Tus ramas: `sql/p1-*`.

> Los acuerdos que comparten las tres partes (estructura, contratos, CLI,
> códigos de error y pruebas) están en el **Anexo** al final. Léelo primero.

## 1. De qué eres dueño

Todo lo que pasa **antes de ejecutar**, más la cara visible del proyecto:

1. La **gramática SQL** (`grammar/SQL.g4`) y el parser de ANTLR.
2. El **AST** propio: sus nodos, el builder que lo arma desde el parse tree y
   sus impresores (texto y `.dot`).
3. Las salidas de visualización: **tokens**, **árbol sintáctico** y **AST**.
4. El **análisis semántico**, que valida cada sentencia contra el catálogo y
   anota el AST con tipos y referencias resueltas.
5. Los **diagnósticos**: la estructura `Diagnostic`, los códigos, los mensajes
   con línea y columna, y el reporter.
6. La **CLI** (`src/main.cpp`).
7. La **IDE** (`ide/sql_ide.py`).

Eres la base de la que dependen los demás. La gramática y `nodes.h` deben
estar estables en la semana 1.

## 2. Archivos

| Archivo | Contenido |
|---|---|
| `grammar/SQL.g4` | Gramática completa, lexer y parser en un solo archivo. |
| `src/frontend/parser_driver.{h,cpp}` | Corre lexer y parser, junta errores y devuelve el parse tree. |
| `src/frontend/error_listener.{h,cpp}` | Convierte los errores de ANTLR en `Diagnostic` (`LEX0xx`/`SYN001`), traducidos al español. |
| `src/frontend/token_dump.{h,cpp}` | `--emit tokens`. |
| `src/frontend/parse_tree_dot.{h,cpp}` | `--emit parse-tree`: recorre el `ParseTree` de ANTLR y genera `.dot`. |
| `src/frontend/ast_builder.{h,cpp}` | Visitor `SQLBaseVisitor` → nodos de `ast::`. |
| `src/ast/nodes.h` | Definición del AST (contrato con la Parte 3). |
| `src/ast/printer.{h,cpp}` | `--emit ast` (texto indentado). |
| `src/ast/ast_dot.{h,cpp}` | `--emit ast-dot`. |
| `src/diagnostics/{diagnostic.h,codes.h,reporter.cpp}` | Diagnósticos y catálogo de códigos. |
| `src/semantic/analyzer.{h,cpp}` | Punto de entrada `analyze(stmt, catalog)`. |
| `src/semantic/scope.{h,cpp}` | Alcances de nombres: tablas del `FROM`, alias y consultas externas. |
| `src/semantic/type_rules.{h,cpp}` | Compatibilidad de tipos y tipo resultante de cada operador. |
| `src/main.cpp` | Lectura de argumentos y despacho a `--emit`, `--json` y `--no-exec`. |
| `ide/sql_ide.py` | IDE en Tkinter. |
| `tests/sql/casos/{lexico,sintactico,semantico}/` | Tus casos de prueba. |

## 3. Qué consumes y qué entregas

**Consumes:**
- De la Parte 2: `Catalog` (solo lectura), `SqlType`, `TypeKind` y `parseDate`.
  Los usa el semántico.
- De la Parte 3: `Session::run(...)`. `main.cpp` la llama para ejecutar, y su
  `StatementResult` se serializa a JSON en el código de la Parte 3.

**Entregas:**
- A la Parte 3:
  - el AST **anotado** (`Expr::type`, `ColumnRef::binding`, `Select::isCorrelated`);
  - la garantía de que, si `analyze` no devolvió errores, la sentencia es
    ejecutable sin más validaciones de nombres ni de tipos.
- A la Parte 2: el AST de `CREATE`, `DROP`, `INSERT`, `UPDATE` y `DELETE`, ya
  validado. Por ejemplo, en un `INSERT` sin lista de columnas, el semántico
  completa la lista con el orden del esquema para que la Parte 2 no tenga que
  hacerlo.
- A todos: `Diagnostic`, `SqlError` y `codes.h`.

## 4. Entrega 1

### 4.1 Esqueleto (semana 0, antes que todo)

- [ ] Crear la rama `sql` desde `main`.
- [ ] Borrar el código de Compiscript: `src/semantic`, `src/codegen`, `src/ir`,
  `src/ast`, `src/frontend`, `src/compiler`, `grammar/Compiscript.g4`,
  `tests/` e `ide/ide_app.py`. **Antes de borrar la IDE, copia lo reutilizable
  a `ide/sql_ide.py`.**
- [ ] Adaptar el `Makefile`:
  - `GRAMMAR := grammar/SQL.g4`;
  - binario `sqlint`;
  - tomar los `.cpp` con `wildcard` (`src/*.cpp` y `src/*/*.cpp`), para que
    nadie más tenga que tocar el `Makefile` al agregar archivos;
  - un `.o` por archivo en `build/`, para no recompilar el parser generado en
    cada cambio;
  - objetivos `make`, `make run FILE=... DB=...`, `make test` y `make test-update`.
- [ ] `tools/setup.sh` queda casi igual; solo se actualizan los comentarios.
- [ ] Revisar `.gitignore`: `generated/`, `.deps/`, `sqlint` y los temporales
  de pruebas.
- [ ] Subir `diagnostic.h`, `codes.h` con todos los códigos de A.8 y un
  `nodes.h` preliminar.

### 4.2 Gramática (`SQL.g4`)

Escríbela **completa desde la entrega 1**, incluidas las construcciones de la
entrega 2 (JOIN, GROUP BY, subconsultas e índices). Rehacer la gramática a
mitad de camino rompe el AST y los visitors de todos. En la entrega 1 el
semántico puede responder "no soportado todavía" a esas construcciones.

- [ ] `options { caseInsensitive = true; }` (disponible desde ANTLR 4.10;
  usamos 4.13.2). Así `select`, `SELECT` y `Select` son la misma palabra.
- [ ] **Léxico:**
  - identificadores: `[a-zA-Z_][a-zA-Z0-9_]*`;
  - enteros y flotantes (`12`, `3.14`, `.5`);
  - cadenas con comilla simple, donde `''` es una comilla escapada;
  - `TRUE`, `FALSE` y `NULL`;
  - comentarios `-- …` y `/* … */`;
  - un token `ERROR_CHAR` al final, para reportar `LEX001` en lugar de que
    ANTLR lo descarte en silencio;
  - una cadena sin cerrar produce `LEX002`.
- [ ] **Script:** `script : (statement ';')* statement? EOF ;`. El `;` final
  es opcional.
- [ ] **DDL:**
  - `CREATE TABLE t (col tipo [NOT NULL] [PRIMARY KEY], ...)`;
  - opcional: `PRIMARY KEY (col)` al final de la lista;
  - `DROP TABLE t`;
  - `CREATE INDEX nombre ON t (col)`;
  - `DROP INDEX nombre`.
- [ ] **Tipos:** `INT`, `FLOAT`, `VARCHAR '(' INT ')'`, `BOOLEAN`, `DATE`.
- [ ] **DML:**
  - `INSERT INTO t [(cols)] VALUES (...), (...)`, con varias filas;
  - `UPDATE t SET c = expr, ... [WHERE expr]`;
  - `DELETE FROM t [WHERE expr]`.
- [ ] **`SELECT`:**
  - lista de selección: `*`, `t.*`, o `expr [AS alias]`;
  - `FROM t [AS] a`;
  - `(INNER JOIN t2 [AS] b ON expr)*`; `JOIN` a secas equivale a `INNER JOIN`;
  - `[WHERE expr] [GROUP BY expr, ...] [HAVING expr] [ORDER BY expr [ASC|DESC], ...]`.
- [ ] **Expresiones** con precedencia por orden de alternativas, de mayor a
  menor:
  1. primario: literal, columna, `(expr)`, `(subconsulta)` escalar, agregado;
  2. unario `-` y `+`;
  3. `* / %`;
  4. `+ -`;
  5. comparación `= <> != < <= > >=`, `IS [NOT] NULL`, `[NOT] IN (lista)`,
     `[NOT] IN (subconsulta)`, `[NOT] EXISTS (subconsulta)`;
  6. `NOT`;
  7. `AND`;
  8. `OR`.
- [ ] **Agregados:** `COUNT(*)`, y `COUNT`, `SUM`, `AVG`, `MIN`, `MAX` de una
  `expr`.
- [ ] **Literal de fecha:** se acepta `DATE '2026-10-01'`. Además, una cadena
  `'2026-10-01'` comparada o insertada en una columna `DATE` se convierte a
  fecha en el semántico.
- [ ] **Pruebas de la gramática:** todos los ejemplos del PDF tienen que
  parsear, incluido el de la subconsulta `IN` de la página 3.

### 4.3 AST y builder

- [ ] Nodos de A.6, todos con `SourceLoc`. Usa `std::unique_ptr` para los
  hijos.
- [ ] `Select` con:
  - `items`;
  - `from` (una `TableRef` con nombre y alias);
  - `joins` (`TableRef` más la condición `on`);
  - `where`, `groupBy`, `having` y `orderBy` (expresión más `asc`/`desc`).
- [ ] El builder **normaliza** las variantes:
  - `JOIN` y `INNER JOIN` dan el mismo nodo;
  - `!=` y `<>` dan el mismo operador;
  - `NOT IN` es un `InList`/`InSubquery` con `negated = true`.
- [ ] Impresor de texto (`--emit ast`) y `.dot` (`--emit ast-dot`): un nodo por
  sentencia, con etiquetas legibles como `Binary(>=)` o `ColumnRef(estudiantes.edad)`.

### 4.4 Tokens y árbol sintáctico

- [ ] `--emit tokens`: tabla con `tipo`, `lexema`, `línea` y `columna`, sin
  incluir los tokens de canales ocultos.
- [ ] `--emit parse-tree`: `.dot` del árbol **de ANTLR**, con reglas como nodos
  internos y tokens como hojas. Es distinto del AST, y la IDE muestra los dos
  (el PDF pide ambos).

### 4.5 Análisis semántico (entrega 1: una sola tabla, sin agregados)

Entrada: una sentencia y el `Catalog` actual. Salida: lista de `Diagnostic`.
Si la lista sale vacía, el AST queda anotado.

- [ ] **Alcance (`scope`):** pila de alcances. Cada consulta abre uno con sus
  fuentes, que son las tablas del `FROM`/`JOIN` con su alias. Desde la
  entrega 1, la resolución de `col` y `x.col` ya recorre la pila hacia afuera:
  en la entrega 2 se necesita para las subconsultas correlacionadas, y es más
  fácil dejarlo listo ahora.
- [ ] **Resolución de nombres:** `SEM001`, `SEM002`, `SEM003`, `SEM004` y `SEM005`.
- [ ] **Reglas de tipos (`type_rules`):**
  - `INT` y `FLOAT` se mezclan; el resultado es `FLOAT`.
  - `+ - * / %` solo entre numéricos. `INT / INT` da `INT` (división entera,
    hay que documentarlo).
  - Se comparan:
    - numérico con numérico;
    - `VARCHAR` con `VARCHAR`;
    - `DATE` con `DATE`, o con una cadena que sea fecha válida;
    - `BOOLEAN` con `BOOLEAN`.
  - `NULL` es compatible con todo.
  - `AND`, `OR`, `NOT` y el `WHERE` exigen `BOOLEAN` (`SEM007`).
  - Los errores de comparación son `SEM006`; los de aritmética y lógica,
    `SEM007`.
- [ ] **`CREATE TABLE`:** `SEM013` y `SEM014`.
- [ ] **`DROP TABLE`:** `SEM001` si la tabla no existe.
- [ ] **`INSERT`:**
  - `SEM008` (cantidad de valores);
  - `SEM009` (tipo); un `INT` en una columna `FLOAT` es válido, al revés no;
  - `SEM010` (longitud), `SEM011` (fecha) y `SEM012` (`NULL` en `NOT NULL`);
  - `SEM015` (columna repetida en la lista).
  - Las columnas omitidas en la lista quedan en `NULL`. Si alguna es
    `NOT NULL`, da `SEM012`.
- [ ] **`UPDATE`:** columnas del `SET` existentes, tipos compatibles, `SEM015`
  si una columna se repite en el `SET`, y `WHERE` booleano.
- [ ] **`DELETE`:** el `WHERE` debe ser booleano.
- [ ] **`SELECT` de una tabla:** `*`, columnas, expresiones, alias, `WHERE` y
  `ORDER BY`. En `ORDER BY` se puede usar un alias de la lista de selección o
  una columna de la tabla.
- [ ] **Mensajes:** concretos y en español, siempre con el nombre del objeto.
  Por ejemplo: *"La columna 'edad2' no existe en 'estudiantes'. Columnas
  disponibles: id, nombre, edad, fecha_ingreso"*. La ubicación es la línea y
  columna del **token problemático**, no del inicio de la sentencia.

### 4.6 CLI (`main.cpp`)

- [ ] Opciones de A.7.
- [ ] Si hay errores léxicos o sintácticos, se reportan **todos** y no se
  ejecuta nada.
- [ ] Sin errores, se llama a `Session::run` (Parte 3). Mientras la Parte 3 no
  esté lista, un stub que solo corra el semántico.
- [ ] Código de salida: `0` si todo bien, `1` si hubo cualquier diagnóstico.

### 4.7 IDE (`ide/sql_ide.py`), versión de la entrega 1

Reutiliza de `ide/ide_app.py` (rama `main`) el editor con números de línea, la
detección de WSL, la llamada al binario, el estado persistente y el arrastrar
y soltar archivos.

- [ ] **Editor SQL** con resaltado de palabras clave, cadenas, números y
  comentarios.
- [ ] **Selector de base de datos**: un directorio, con `databases/escuela`
  por defecto.
- [ ] Botones **Ejecutar todo** y **Ejecutar selección** (F5 y Ctrl+Enter).
- [ ] Pestañas:
  - **Tokens:** `ttk.Treeview` con tipo, lexema, línea y columna.
  - **Árbol sintáctico:** PNG generado con `dot` desde `--emit parse-tree`.
    Si no hay Graphviz, se muestra el texto.
  - **AST:** igual, desde `--emit ast-dot`.
  - **Errores:** tabla con fase, código, línea, columna y mensaje. **Doble
    clic lleva a la línea** en el editor y subraya en rojo la posición.
  - **Resultados:** una sub-pestaña por sentencia. Las consultas `SELECT` se
    muestran en una grilla `ttk.Treeview`; las demás, como mensaje (por
    ejemplo, "3 filas actualizadas").
- [ ] La IDE **solo** habla con `sqlint` mediante `--json` y `--emit`. No lee
  los CSV directamente; así no duplica lógica.

## 5. Entrega 2

### 5.1 Semántico de lo que falta

- [ ] **`JOIN`:**
  - varias fuentes en el alcance;
  - la ambigüedad (`SEM003`) se vuelve real;
  - `t.*` se expande solo a las columnas de `t`;
  - la condición `ON` debe ser booleana.
- [ ] **Agregados y `GROUP BY`:**
  - `SEM020`: agregado en `WHERE`, o agregados anidados;
  - `SEM021`: si hay `GROUP BY` o algún agregado en el `SELECT`, toda columna
    que aparezca fuera de un agregado (en `SELECT`, `HAVING` u `ORDER BY`)
    debe estar en el `GROUP BY`;
  - `SEM022`: `SUM`/`AVG` sobre algo no numérico.
  - Tipos de resultado: `COUNT` da `INT`; `AVG` da `FLOAT`; `SUM` da el tipo
    del argumento; `MIN` y `MAX` dan el tipo del argumento.
  - `HAVING` sin `GROUP BY` es válido: toda la tabla cuenta como un grupo.
- [ ] **Subconsultas:**
  - se abre un alcance nuevo encima del externo;
  - una `ColumnRef` que se resuelve afuera recibe `depth > 0`, y la consulta
    interna se marca `isCorrelated`;
  - `SEM018`: subconsulta escalar con más de una columna (cuenta `*`
    expandido);
  - `SEM019`: subconsulta de `IN` con más de una columna;
  - el tipo de la columna de la subconsulta debe ser compatible con el lado
    izquierdo (`SEM006`).
- [ ] **Índices:** `SEM016` (mismo nombre, o ya hay un índice sobre esa
  columna), `SEM017`; y en `CREATE INDEX`, `SEM001`/`SEM002` si la tabla o la
  columna no existen.

### 5.2 IDE: pulido final

- [ ] Pestaña **Plan**: muestra el campo `plan` de cada sentencia, por ejemplo
  `IndexScan idx_edad` o `SeqScan`. Sirve en la presentación para
  **demostrar** que se usan los índices.
- [ ] Panel **Catálogo**: árbol con tablas, sus columnas con tipo y
  restricciones, y sus índices. Se refresca después de cada ejecución. Para
  leerlo hace falta una opción `--emit catalog`; coordínala con la Parte 2.
- [ ] Ejemplos precargados en un menú: los del PDF y uno por cada tema.
- [ ] Atajos de teclado, el último archivo abierto, y conservar el tamaño y la
  posición de la ventana.

## 6. Pruebas que te tocan

Al menos un caso **exitoso** y uno **fallido** por cada código `LEX*`, `SYN*` y
`SEM*` de A.8. Los fallidos verifican **código, línea y columna**. Además:

- [ ] Que todos los ejemplos del PDF parseen.
- [ ] Mayúsculas y minúsculas: `select NOMBRE from Estudiantes` funciona.
- [ ] Un `CREATE TABLE` seguido de `INSERT` en el mismo script no da `SEM001`;
  esto prueba el flujo sentencia por sentencia.
- [ ] Varios errores sintácticos en un script: se reportan todos.

## 7. Cuándo está terminado

- Cada regla semántica tiene su código, su mensaje con ubicación y sus dos
  casos de prueba.
- La IDE puede escribir SQL, ver tokens, árbol sintáctico, AST y errores,
  ejecutar y ver resultados. Son los 7 puntos de "Interfaz Gráfica" del PDF.
- `--emit` funciona para todos los modos y la IDE no lee archivos de la base
  directamente.

## 8. Riesgos y consejos

- **Recursión izquierda en `expr`:** ANTLR4 la acepta directamente. El orden de
  las alternativas define la precedencia, así que pruébala con
  `a OR b AND c` y `NOT a = b`.
- **`IN (lista)` vs `IN (subconsulta)`:** pon primero la alternativa con
  `selectStmt`; si no, `(SELECT ...)` se intenta parsear como lista de
  expresiones.
- **Ambigüedad de alias:** en `SELECT e.nombre FROM estudiantes e`, la tabla
  ya **no** se puede nombrar como `estudiantes.nombre`. Es el comportamiento
  estándar; decídelo y documéntalo.
- Lo que no se valida en el semántico lo tiene que validar la Parte 3 en
  ejecución. Cualquier hueco termina en un crash en la presentación.

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
