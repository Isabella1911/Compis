# sqlint: intérprete SQL con almacenamiento en archivos

Proyecto de *Construcción de Compiladores* (UVG, 2026). Interpreta y ejecuta
sentencias SQL guardando las bases de datos en archivos de texto, sin usar
ningún DBMS.

```
SQL → léxico → sintáctico → AST → semántico → interpretación → archivos → resultado
```

> Esta es la rama `sql`. El proyecto anterior (Compiscript) sigue en `main`.

## Compilar y correr (WSL/Linux)

Una sola vez por computadora:

```bash
bash tools/setup.sh
```

Después:

```bash
make
```

```bash
./sqlint --db databases/escuela consulta.sql
```

## Línea de comandos

```
sqlint [--db <dir>] [--json] [--emit tokens|parse-tree|ast|ast-dot] [--no-exec] <archivo.sql | ->
```

| Opción | Qué hace |
|---|---|
| `--db <dir>` | Directorio de la base de datos (por defecto `databases/escuela`). |
| `--json` | Toda la salida como un objeto JSON; es lo que usa la IDE. |
| `--emit tokens` | Lista de tokens: tipo, lexema, línea y columna. |
| `--emit parse-tree` | Árbol sintáctico de ANTLR en formato Graphviz (`.dot`). |
| `--emit ast` | AST como árbol de texto, con línea y columna de cada nodo. |
| `--emit ast-dot` | AST en formato Graphviz (`.dot`). |
| `--no-exec` | Solo análisis léxico, sintáctico y semántico. |

Con `-` en lugar del archivo, el script se lee de la entrada estándar.

Mientras no esté la `Session` de la Parte 3, `sqlint` solo analiza: corre el
semántico sentencia por sentencia sobre el catálogo de la base
(`metadata.txt`) y no escribe nada en disco. El almacenamiento de la Parte 2
(`src/storage/`, `src/index/`) ya está y se prueba con `make test-storage` y
`make test-mutator`; cómo se conecta está en
[docs/sql/parte2_implementacion.md](docs/sql/parte2_implementacion.md).

## Pruebas

Casos de la Parte 1 (un caso exitoso y uno fallido por regla, con el código
y la posición exacta del error) en `tests/sql/casos/`:

```bash
make test-parte1
```

Almacenamiento (Parte 2): valores, CSV, catálogo e índices sin ANTLR, y el
`Mutator` con scripts reales (incluye que `databases/escuela` sea exactamente
lo que produce `databases/escuela/crear.sql`):

```bash
make test-storage
```

```bash
make test-mutator
```

Batería completa (compara la salida `--json` con los `.expected`):

```bash
make test
```

## Estado

| Etapa | Estado |
|---|---|
| Gramática completa (`grammar/SQL.g4`), tokens, árbol sintáctico, errores léxicos y sintácticos | Hecho |
| AST (nodos, builder, impresor de texto y `.dot`) | Hecho |
| Análisis semántico de la entrega 1 (sin JOIN, agregados, subconsultas ni índices) | Hecho |
| Almacenamiento: valores, CSV, catálogo, `INSERT`/`UPDATE`/`DELETE`, índices, base de ejemplo y runner (Parte 2) | Hecho |
| Ejecución de consultas y `Session` (Parte 3) | Pendiente |
| IDE | Pendiente |

## Documentación

El reparto del trabajo y los contratos entre partes están en `docs/sql/`:

- [Parte 1: gramática, AST, semántica e IDE](docs/sql/parte1_gramatica_semantica_ide.md)
- [Parte 2: almacenamiento e índices](docs/sql/parte2_almacenamiento_indices.md)
- [Parte 3: ejecución de consultas](docs/sql/parte3_ejecucion_consultas.md)
