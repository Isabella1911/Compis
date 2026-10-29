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
sqlint [--db <dir>] [--json] [--emit tokens|parse-tree] [--no-exec] <archivo.sql | ->
```

| Opción | Qué hace |
|---|---|
| `--db <dir>` | Directorio de la base de datos (por defecto `databases/escuela`). |
| `--json` | Toda la salida como un objeto JSON; es lo que usa la IDE. |
| `--emit tokens` | Lista de tokens: tipo, lexema, línea y columna. |
| `--emit parse-tree` | Árbol sintáctico de ANTLR en formato Graphviz (`.dot`). |
| `--no-exec` | Solo análisis léxico, sintáctico y semántico. |

Con `-` en lugar del archivo, el script se lee de la entrada estándar.

## Estado

| Etapa | Estado |
|---|---|
| Gramática completa (`grammar/SQL.g4`), tokens, árbol sintáctico, errores léxicos y sintácticos | Hecho |
| AST | Pendiente |
| Análisis semántico | Pendiente |
| Almacenamiento, ejecución e índices (Partes 2 y 3) | Pendiente |
| IDE | Pendiente |

## Documentación

El reparto del trabajo y los contratos entre partes están en `docs/sql/`:

- [Parte 1: gramática, AST, semántica e IDE](docs/sql/parte1_gramatica_semantica_ide.md)
- [Parte 2: almacenamiento e índices](docs/sql/parte2_almacenamiento_indices.md)
- [Parte 3: ejecución de consultas](docs/sql/parte3_ejecucion_consultas.md)
