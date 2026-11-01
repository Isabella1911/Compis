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
sqlint [--db <dir>] [--json] [--emit tokens|parse-tree|ast|ast-dot|catalog] [--no-exec] <archivo.sql | ->
```

| Opción | Qué hace |
|---|---|
| `--db <dir>` | Directorio de la base de datos (por defecto `databases/escuela`). |
| `--json` | Toda la salida como un objeto JSON; es lo que usa la IDE. |
| `--emit tokens` | Lista de tokens: tipo, lexema, línea y columna. |
| `--emit parse-tree` | Árbol sintáctico de ANTLR en formato Graphviz (`.dot`). |
| `--emit ast` | AST como árbol de texto, con línea y columna de cada nodo. |
| `--emit ast-dot` | AST en formato Graphviz (`.dot`). |
| `--emit catalog` | Catálogo de tablas, columnas e índices en JSON. No requiere archivo SQL. |
| `--no-exec` | Solo análisis léxico, sintáctico y semántico. |

Con `-` en lugar del archivo, el script se lee de la entrada estándar.

`sqlint` analiza y ejecuta cada sentencia contra el catálogo actualizado. Se
detiene en el primer error semántico o de ejecución; las sentencias anteriores
quedan guardadas. Los errores léxicos o sintácticos impiden ejecutar todo el
script. Con `--no-exec`, el efecto de los DDL se simula en memoria sin escribir.

Ejemplo de consulta (solo lectura):

```bash
./sqlint --json --db databases/escuela - <<'SQL'
SELECT e.nombre, COUNT(*) AS cursos
FROM estudiantes e
JOIN inscripciones i ON i.estudiante_id = e.id
GROUP BY e.nombre
ORDER BY cursos DESC;
SQL
```

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

Ejecución (Parte 3): contextos, caché, lógica de NULL, SELECT, JOIN,
agregados, subconsultas y planes con índices:

```bash
make test-exec
```

Batería completa (incluye las verificaciones anteriores y compara la salida
`--json` con los `.expected` disponibles):

```bash
make test
```

## IDE

Interfaz gráfica en Tkinter. Habla con `sqlint` solo mediante `--json` y
`--emit`: nunca lee los CSV directamente. Si `sqlint` no está compilado, lo
compila con `make` la primera vez que se ejecuta algo.

```bash
python3 ide/sql_ide.py
```

**Qué incluye:**
- **Editor** con resaltado de SQL y números de línea.
- **Ejecución:** `F5` ejecuta todo el script, `Ctrl+Enter` solo la selección
  y `F6` solo analiza, sin tocar la base.
- **Pestañas:**
  - **Resultados**, una sub-pestaña por sentencia, con una grilla para cada `SELECT`.
  - **Errores**: doble clic lleva a la posición del error y la subraya.
  - **Plan**: `SeqScan` o `IndexScan`.
  - **Tokens**.
  - **Árbol sintáctico** y **AST**, renderizados con Graphviz. Sin
    Graphviz se muestran como texto.
- **Panel Catálogo:** tablas, columnas, restricciones e índices.
- **Menú Ejemplos:** un script por tema, en `ide/ejemplos/`.

**La base no se modifica por defecto.** El IDE ejecuta sobre una **copia** de
la base elegida, en `output/ide/bases/`, para que probar no modifique
`databases/escuela`, que está versionada. La copia se reinicia desde el
original con el botón "Reiniciar copia". Si se desmarca "usar copia", el IDE
trabaja sobre la base original.

El arrastrar y soltar archivos es opcional; requiere `pip install tkinterdnd2`.

## Estado

| Etapa | Estado |
|---|---|
| Gramática completa (`grammar/SQL.g4`), tokens, árbol sintáctico, errores léxicos y sintácticos | Hecho |
| AST (nodos, builder, impresor de texto y `.dot`) | Hecho |
| Análisis semántico con JOIN, agregados, subconsultas e índices | Hecho |
| Almacenamiento: valores, CSV, catálogo, `INSERT`/`UPDATE`/`DELETE`, índices, base de ejemplo y runner (Parte 2) | Hecho |
| Ejecución de consultas, `Session`, planes y salida JSON/tabular (Parte 3) | Hecho |
| IDE (`ide/sql_ide.py`) | Hecho |

## Documentación

El reparto del trabajo y los contratos entre partes están en `docs/sql/`:

- [Parte 1: gramática, AST, semántica e IDE](docs/sql/parte1_gramatica_semantica_ide.md)
- [Parte 2: almacenamiento e índices](docs/sql/parte2_almacenamiento_indices.md)
- [Parte 3: ejecución de consultas](docs/sql/parte3_ejecucion_consultas.md)
- [Implementación e integración de la Parte 3](docs/sql/parte3_implementacion.md)
