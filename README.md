# Compiscript — Proyecto 2

Analizador **sintáctico y semántico** de [Compiscript](grammar/Compiscript.g4)
(subconjunto de TypeScript) para *Construcción de Compiladores*, UVG.

| | |
|---|---|
| **Repositorio** | <https://github.com/Isabella1911/Compis> |
| **Rama principal** | `main` |
| **Entrada** | archivos `.cps` |
| **Salida** | diagnósticos · AST (texto + DOT) · tabla de símbolos |
| **Stack** | ANTLR 4 → AST propio (C++17) → pases semánticos · IDE Python/Tkinter |

El Visitor de ANTLR **solo** construye el AST. Las reglas semánticas se aplican
en pases posteriores sobre nodos propios (desacoplados de ANTLR).

> Trabajo anterior (YALex/YAPar, LL/LR): rama
> [`Deprecated`](../../tree/Deprecated). No es continuidad de ese código.

---

## Índice

1. [Quick start](#1-quick-start)
2. [Pipeline](#2-pipeline)
3. [Estructura del repo](#3-estructura-del-repo)
4. [Tabla de símbolos](#4-tabla-de-símbolos)
5. [Códigos de diagnóstico](#5-códigos-de-diagnóstico)
6. [Reglas semánticas ↔ enunciado](#6-reglas-semánticas--enunciado)
7. [Decisiones de lenguaje](#7-decisiones-de-lenguaje)
8. [IDE](#8-ide)
9. [Batería de pruebas](#9-batería-de-pruebas)
10. [Entregables y rúbrica](#10-entregables-y-rúbrica)
11. [Documentación](#11-documentación)
12. [Fuera de Proyecto 2](#12-fuera-de-proyecto-2)

---

## 1. Quick start

**Primera vez** (JRE, `antlr.jar`, CMake y runtime C++ de ANTLR en `.deps/`, sin sudo):

```bash
bash tools/setup.sh
```

**Uso diario:**

```bash
make                                            # genera parser (si hace falta) y compila
make run FILE=tests/fixtures/valid/hello.cps    # analiza un .cps
make test                                       # unitarios + fixtures (códigos exactos)
python3 ide/ide_app.py                          # IDE
```

| Comando | Qué hace |
|---------|----------|
| `make` / `make build` | Binario `compiscript` |
| `make run FILE=…` | CLI sobre un `.cps` |
| `make test` | symbols + semantic + runtime + runner + fixtures |
| `make test-symbols` | Scope/Symbol (sin ANTLR) |
| `make test-semantic` | Regresiones AST/capturas (sin ANTLR) |
| `make test-runtime` | Heap + mark-and-sweep (fuera de rúbrica P2) |
| `make clean` | Binarios y `generated/` |
| `make -C docs/informe` | PDF del informe formal |

El CLI imprime diagnósticos, AST indentado, tabla de símbolos y escribe
`output/ast.dot`. Render opcional:

```bash
dot -Tpng output/ast.dot -o ast.png
```

Requisitos del host: `g++` C++17, Python 3.8+ (Tkinter para la IDE). Graphviz
y `tkinterdnd2` son opcionales. Setup pensado para Linux x86-64.

---

## 2. Pipeline

```text
.cps
  → ANTLR (lexer + parser)
  → AstBuilder (Visitor)          → AST propio
  → DeclarationCollector          → scopes + símbolos
  → InheritanceResolver           → bases / ciclos
  → NameResolver                  → SEM001
  → TypeChecker                   → firmas + tipos + clases/args
  → ControlFlowChecker            → break/continue/return/muerto
  → ClosureAnalyzer               → capturas (metadatos)
  → CompilationResult { ast, diagnostics, symbol_table }
```

Fachada: `Compiler::compile(source)` en `src/compiler/`. Sin estado global
entre llamadas (seguro para la IDE). Error léxico/sintáctico → sin AST; errores
semánticos permiten seguir acumulando diagnósticos.

Detalle de pases: [docs/03_passes_semanticos.md](docs/03_passes_semanticos.md).

---

## 3. Estructura del repo

```text
├── grammar/Compiscript.g4          # gramática oficial (sin modificar)
├── generated/                      # ANTLR (gitignored; `make` lo regenera)
├── src/
│   ├── frontend/                   # ÚNICO lugar con headers ANTLR
│   │   ├── parser_driver.*         # source → AST
│   │   ├── ast_builder.*           # Visitor Parse Tree → AST
│   │   └── error_listener.*        # ANTLR → Diagnostic
│   ├── ast/                        # nodos + printTree / toDot
│   ├── diagnostics/                # Diagnostic, codes.h, Reporter
│   ├── compiler/                   # Compiler::compile, CompilationResult
│   ├── semantic/                   # símbolos, tipos, pases
│   └── runtime/                    # fuera de P2: heap + GC
├── ide/ide_app.py                  # IDE (subproceso sobre el binario)
├── tests/fixtures/{valid,invalid}/
├── tests/fixtures/expected.json    # códigos exactos por fixture inválido
├── docs/                           # guías + informe LaTeX
├── tools/setup.sh
└── Makefile
```

**Regla:** fuera de `src/frontend/` no se incluyen tipos de ANTLR.

---

## 4. Tabla de símbolos

Pila de tablas (`Scope` con padre/hijos):

| Kind | Uso |
|------|-----|
| Global | Programa |
| Function | Parámetros + cuerpo (scope compartido con params) |
| Class | Atributos / métodos |
| Block | Bloques anidados |

- `Symbol` / `FunctionSymbol` / `ClassSymbol`
- `declare` / `resolve` / `resolveLocal` — shadowing OK; redeclaración local → `SEM002`
- Símbolos rechazados se conservan para recuperación sin contaminar búsquedas
- Closures: `FunctionSymbol::captured` y `captured_this` (impresos como `[captura: …]`)

Cubre el componente de rúbrica **Tabla de símbolos (25 pts)**.

---

## 5. Códigos de diagnóstico

Definidos en `src/diagnostics/codes.h`. La batería compara **códigos**, no mensajes.

| Código | Significado |
|--------|-------------|
| `SYN001` | Error sintáctico (ANTLR) / `const` sin inicializar |
| `SEM001` | Identificador no declarado |
| `SEM002` | Redeclaración en el mismo ámbito |
| `SEM003` | Asignación incompatible o destino no asignable/`const` |
| `SEM004` | Tipo incompatible en operación / comparación / elementos / índice |
| `SEM005` | Condición no booleana (`if`/`while`/`do-while`/`for`/ternario) |
| `SEM006` | `break`/`continue` fuera de bucle (`switch` no cuenta) |
| `SEM007` | `return` fuera de función |
| `SEM008` | Número o tipo de argumentos incorrecto |
| `SEM009` | Tipo de retorno incompatible |
| `SEM010` | Miembro inexistente |
| `SEM011` | Parámetro sin anotación de tipo |
| `SEM012` | Código muerto estructural |
| `SEM013` | Tipo / base de herencia inválidos |
| `SEM014` | Herencia circular |
| `SEM015` | `this` fuera de clase |
| `SEM016` | Valor no invocable |
| `SEM017` | `new` sobre no-clase / constructor inválido |
| `SEM018` | `foreach` sobre no-arreglo |
| `SEM019` | Tipo no inferible / `void` usado como dato |
| `SEM020` | Función con retorno puede terminar sin devolver |

---

## 6. Reglas semánticas ↔ enunciado

| Área del PDF | Cobertura |
|--------------|-----------|
| Tipos aritméticos / lógicos / comparaciones | `SEM004` (`integer`; no hay `float` en la gramática) |
| Asignaciones y `const` | `SEM003`, `SYN001` |
| Listas / índices / `foreach` | `SEM004`, `SEM018`, `SEM019` |
| Ámbitos, no declarada, redeclaración | `SEM001`, `SEM002`, scopes anidados |
| Args, retorno, recursión, anidadas | `SEM008`, `SEM009`, `SEM020`, `ClosureAnalyzer` |
| Condiciones, `break`/`continue`/`return`, muerto | `SEM005`–`SEM007`, `SEM012` |
| Clases, `.`, constructor, `this` | `SEM010`, `SEM008`, `SEM015`, `SEM013`/`SEM014` |
| Expresiones sin sentido, duplicados | `SEM016`, `SEM002` |

---

## 7. Decisiones de lenguaje

Verificadas contra `Compiscript.g4` (no asumidas):

1. **No existe `float`.** `baseType` = `boolean` \| `integer` \| `string` \| `Identifier`.
2. **Parámetros con anotación** semántica (`SEM011`); sin retorno anotado → `Void` interno.
3. **`catch (id)`** sin tipo en la gramática → tipo interno `string`.
4. **`switch`:** se valida compatibilidad sujeto/casos (`SEM004`). El PDF pide sujeto booleano — discrepancia documentada.
5. **`switch` no es bucle** → `break`/`continue` ahí → `SEM006`.
6. **`string + integer` prohibido**; `+` es entero+entero o string+string.
7. **No hay `throw`**; `try`/`catch` se valida igual.
8. **`if`/`while`/`for`/… exigen `{ }`** (la gramática lo impone).
9. **Constructor** = método llamado `constructor`.
10. El lexer colapsa literales en el token `Literal`; `AstBuilder` distingue entero/string por el texto.

Políticas de `const`, `this` y capturas: [docs/03_passes_semanticos.md](docs/03_passes_semanticos.md).

---

## 8. IDE

```bash
python3 ide/ide_app.py
```

| Función | Detalle |
|---------|---------|
| Editor `.cps` | Abrir / editar / guardar; fixtures en panel Proyecto |
| Compilar (`F5`) | `make` → binario |
| Ejecutar análisis (`F6`) | `./compiscript <archivo.cps>` |
| Diagnósticos | Código, severidad, línea, columna, mensaje |
| Navegación | Doble clic → posición; líneas de error resaltadas |
| AST | Texto + PNG vía Graphviz (`output/ast.dot`) |
| Símbolos | Árbol de scopes indentado |

No reimplementa el compilador: parsea el stdout del CLI. Guía:
[ide/IDE.md](ide/IDE.md).

---

## 9. Batería de pruebas

| Conjunto | Cantidad (aprox.) |
|----------|-------------------|
| `tests/fixtures/valid/` | ~30 programas aceptados |
| `tests/fixtures/invalid/` | ~110 casos por regla (`sem00X_…cps`) |
| `expected.json` | conjunto **exacto** de códigos por inválido |

```bash
make test
# → test-symbols, test-semantic, test-runtime,
#   fixture_runner_test.py, run_fixtures.py
```

El runner falla si: código de salida incorrecto, stderr inesperado, códigos
distintos a los esperados, posición inválida, crash o timeout (5 s).

---

## 10. Entregables y rúbrica

| Entregable (PDF) | Ubicación |
|------------------|-----------|
| Repo GitHub + commits individuales | <https://github.com/Isabella1911/Compis> |
| Batería por regla | `tests/fixtures/` + `make test` |
| Documentación arquitectura / ejecución | este README + `docs/` |
| IDE funcional | `ide/ide_app.py` |
| Informe formal | [docs/informe/](docs/informe/) (`informe.tex` / `.pdf`) |

| Componente | Pts | Cómo se cubre |
|------------|-----|----------------|
| IDE | 15 | Editor, compilar, análisis, diagnósticos, AST, símbolos |
| Analizador sintáctico y semántico | 60 | ANTLR + AST visual + pases + batería |
| Tabla de símbolos | 25 | Scopes global / función / clase / bloque |
| **Total** | **100** | |

Checklist: [docs/04_ide_y_entrega.md](docs/04_ide_y_entrega.md).

---

## 11. Documentación

| Documento | Contenido |
|-----------|-----------|
| **Este README** | Índice operativo del proyecto |
| [docs/03_passes_semanticos.md](docs/03_passes_semanticos.md) | Pases, inferencia, `this`, closures, límites |
| [docs/04_ide_y_entrega.md](docs/04_ide_y_entrega.md) | IDE + tests + checklist de entrega |
| [docs/05_generacion_de_codigo_y_runtime.md](docs/05_generacion_de_codigo_y_runtime.md) | GC mark-and-sweep (fuera de P2) |
| [ide/IDE.md](ide/IDE.md) | Uso de la interfaz |
| [docs/informe/informe.tex](docs/informe/informe.tex) | Informe del enunciado (carátula, 3 nombres, link repo) |
| [docs/ANALISIS_ESTADO_PROYECTO.md](docs/ANALISIS_ESTADO_PROYECTO.md) | Informe **histórico** (estado anterior; no sustituye este README) |

---

## 12. Fuera de Proyecto 2

No forma parte de la rúbrica de análisis semántico:

- IR / three-address code
- Backend MIPS / ejecución de programas
- Runtime completo en el CLI

Sí hay un **scaffold** en `src/runtime/` (heap, mark-and-sweep, descriptores
desde `ClassSymbol` y capturas de closures). Probar: `make test-runtime`.
Diseño: [docs/05_generacion_de_codigo_y_runtime.md](docs/05_generacion_de_codigo_y_runtime.md).

---

## Estado (resumen)

- [x] Toolchain reproducible (`tools/setup.sh`)
- [x] Parser ANTLR + AST propio + visualización texto/DOT
- [x] Tabla de símbolos con entornos anidados
- [x] Pases semánticos conectados a `Compiler::compile()`
- [x] Diagnósticos `SYN001` + `SEM001`–`SEM020`
- [x] IDE + resaltado de errores
- [x] Batería por regla con códigos exactos
- [x] Informe LaTeX / PDF
- [x] Scaffold runtime/GC (opcional, fuera de rúbrica)
