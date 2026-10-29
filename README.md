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

Mientras no estén las Partes 2 y 3, `sqlint` solo analiza: corre el
semántico sentencia por sentencia sobre un catálogo en memoria (los
`CREATE TABLE` del propio script) y no escribe nada en disco.
`src/storage/value.*` y `src/storage/catalog.*` son *stubs* de la Parte 1
que la Parte 2 reemplaza.

## Pruebas

Casos de la Parte 1 (un caso exitoso y uno fallido por regla, con el código
y la posición exacta del error) en `tests/sql/casos/`:

```bash
make test-parte1
```

## Estado

| Etapa | Estado |
|---|---|
| Gramática completa (`grammar/SQL.g4`), tokens, árbol sintáctico, errores léxicos y sintácticos | Hecho |
| AST (nodos, builder, impresor de texto y `.dot`) | Hecho |
| Análisis semántico de la entrega 1 (sin JOIN, agregados, subconsultas ni índices) | Hecho |
| Almacenamiento, ejecución e índices (Partes 2 y 3) | Pendiente |
| IDE | Pendiente |

## Documentación

El reparto del trabajo y los contratos entre partes están en `docs/sql/`:

- [Parte 1: gramática, AST, semántica e IDE](docs/sql/parte1_gramatica_semantica_ide.md)
- [Parte 2: almacenamiento e índices](docs/sql/parte2_almacenamiento_indices.md)
- [Parte 3: ejecución de consultas](docs/sql/parte3_ejecucion_consultas.md)
