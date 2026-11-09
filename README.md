# Compiscript — Proyecto 2: generación de código intermedio

Compilador de **Compiscript** (subconjunto de TypeScript) para el curso
*Construcción de Compiladores*, UVG. Toma un archivo `.cps`, lo analiza con
**ANTLR**, valida las reglas semánticas y, si no hay errores, genera
**código de tres direcciones (TAC)**. La fase siguiente (Proyecto 3)
traducirá ese TAC a MIPS para correrlo en MARS.

| Documento | Qué contiene |
|---|---|
| Este README | Cómo instalar, compilar, correr, probar y usar la IDE. |
| [`docs/08_lenguaje_intermedio.md`](docs/08_lenguaje_intermedio.md) | **Especificación del TAC**: instrucciones, operandos, registro de activación, tabla de símbolos extendida, temporales y supuestos. |
| [`docs/09_arquitectura.md`](docs/09_arquitectura.md) | **Arquitectura**: pipeline, carpetas y cómo cada fase usa la tabla de símbolos. |
| [`docs/10_ejemplos_de_traduccion.md`](docs/10_ejemplos_de_traduccion.md) | **Ejemplos de traducción**: cada construcción con su `.cps` y el TAC real que genera. |
| [`docs/02_sistema_de_tipos.md`](docs/02_sistema_de_tipos.md), [`docs/03_passes_semanticos.md`](docs/03_passes_semanticos.md) | Diseño del análisis semántico (entrega anterior, sigue vigente). |

## Historial

Este repositorio empezó como `Compis`, un compilador didáctico con lexer y
parser hechos a mano, del curso *Diseño de Lenguajes*. Ese trabajo quedó
preservado en la rama [`Deprecated`](../../tree/Deprecated). El contenido
actual de `main` es un proyecto nuevo (ANTLR + C++).

---

## 1. Pipeline

```
programa.cps
  │
  ▼  src/frontend/        ANTLR (lexer + parser desde grammar/Compiscript.g4)
Parse tree ──► AstBuilder ──► AST propio (src/ast/nodes.h)
  │
  ▼  src/semantic/        6 pases, cada uno lee y completa la tabla de símbolos
DeclarationCollector → InheritanceResolver → NameResolver
  → TypeChecker → ControlFlowChecker → ClosureAnalyzer
  │
  ├──► si hubo errores: diagnósticos [SYN/SEMxxx] con línea y columna, sin TAC
  ▼  src/codegen/         (solo si no hubo ningún error)
FrameLayout: direcciones, offsets, tamaños, etiquetas y frames
  → TacGenerator: AST → TAC (con reciclaje de temporales)
  │
  ▼  src/ir/
tac_printer: TAC → texto (pantalla y output/programa.tac)
```

El detalle de cada fase está en [`docs/09_arquitectura.md`](docs/09_arquitectura.md).

---

## 2. Instalación (una sola vez por máquina)

Requisitos: Linux o **WSL** (en Windows), con `bash`, `make`, `g++`
(C++17), `curl` y `python3`. Para la IDE, además `python3-tk`. Graphviz
(`dot`) es opcional, solo para dibujar el AST.

```bash
sudo apt install build-essential curl python3 python3-tk graphviz   # si falta algo
bash tools/setup.sh
```

`tools/setup.sh` descarga y compila **dentro de `.deps/`** (sin sudo) el
JRE, `antlr-4.13.2-complete.jar`, un `cmake` portátil y el runtime C++ de
ANTLR. Tarda unos minutos la primera vez; después no hace nada.

---

## 3. Compilar y correr

Desde la raíz del repositorio:

```bash
make                                            # genera el parser (si hace falta) y compila ./compiscript
make run FILE=tests/tac/p3_programa_completo.cps
./compiscript --tac tests/tac/p1_temporales.cps # solo el TAC
./compiscript --mips tests/tac/p2_clase.cps -o clase.asm   # ensamblador MIPS
make clean                                      # borra binarios y código generado
```

**Tres modos de salida:**

| Comando | Muestra |
|---|---|
| `./compiscript archivo.cps` | Diagnósticos (si hay), AST, tabla de símbolos extendida y código intermedio. Escribe `output/ast.dot`, `output/programa.tac` y `output/programa.asm`. |
| `./compiscript --tac archivo.cps` | Solo el TAC, o solo los diagnósticos si hubo errores. Es el modo que usan las pruebas. |
| `./compiscript --mips archivo.cps [-o salida.asm]` | Solo el ensamblador MIPS (para MARS), o los diagnósticos si hubo errores (incluidos los del backend, `CG001`–`CG004`). |

Todo pasa por `Compiler::compile()`: ANTLR → AST → semántica → TAC → MIPS;
cada etapa corre solo si las anteriores no tuvieron errores. **Si hay
cualquier error no queda ningún `.asm`**, ni siquiera uno de una
compilación anterior. El backend, su convención de llamada, el runtime y
`try/catch` están documentados en
[`tests/mips/opcode_support.md`](tests/mips/opcode_support.md).

**Código de salida:** `0` si compiló sin errores, `1` si hubo errores. Si
hay errores **no se genera TAC** (paso 4 del enunciado).

Ejemplo de diagnóstico:

```
Diagnosticos:
  [SEM003] error 3:1: el valor asignado no coincide con el tipo de 'x'.
```

---

## 4. Batería de pruebas

```bash
make test             # todo (sin la IDE): símbolos, semántica, TAC, MIPS y CLI; corre todas las suites aunque alguna falle
make test-mips        # backend MIPS: integración (CLI) + extremo a extremo en MARS
make test-mips-unit   # unitarias del backend: TAC armado a mano, errores CGxxx y ejecución en MARS
make test-mips-integration   # solo CLI: .asm generado o rechazado con sus códigos, sin MARS
make test-mips-e2e    # cada programa válido ejecutado en MARS (SKIP si no hay MARS)
make test-mips-estricto      # MIPS, CLI e IDE en modo estricto: exige MARS y display, incluye CLI e IDE, falla en vez de SKIP
make validar-entrega  # TODO en modo estricto + chequeo de requisitos (tests/validar_rubrica.py)
make test-cli         # CLI pública: rutas inexistentes, vacío, tildes y espacios, salida inválida, flags, entrada grande
make test-ide         # IDE con Tkinter real (necesita display; abre la ventana): escenarios A, D y E, F5/F7, MARS
make test-semantic    # solo los casos semánticos
make test-tac         # solo el TAC contra los esperados
make test-tac-update  # regenera los .tac esperados (revisar el diff antes de commitear)
make test-symbols     # solo la prueba unitaria de la tabla de símbolos
```

Al final imprime un resumen `N ok, M fallos` y sale con código `1` si algo
falló. El runner es [`tests/run_tests.py`](tests/run_tests.py) (Python 3,
sin dependencias).

| Carpeta | Qué verifica |
|---|---|
| `tests/fixtures/valid/` | Programas correctos: deben compilar sin diagnósticos. Hay al menos uno por regla (`sem001_…` a `sem016_…`, `syn001_…`). |
| `tests/fixtures/invalid/` | Programas con errores: deben fallar con **exactamente** los códigos de sus líneas `// expect: SEMxxx` (una por error esperado). |
| `tests/tac/*.cps` + `*.tac` | Casos exitosos de TAC (*golden files*): la salida de `--tac` debe ser idéntica al `.tac`. Si difiere, se muestra el `diff`. Hay uno por construcción (`p1_*`, `p2_*`) y un programa completo (`p3_programa_completo`). |
| `tests/tac/errores/` | Casos fallidos de TAC: no debe aparecer TAC y deben salir los códigos de `// expect:`. |
| `tests/symbol_table_test.cpp` | Prueba unitaria de `Scope`/`Symbol`. |
| `tests/mips/valid/` | Programas que deben generar `.asm` y, **ejecutados en MARS**, imprimir exactamente lo de `// expect-out:` y terminar con el código de `// expect-exit:` (0 si no se indica). |
| `tests/mips/invalid/` | Programas con errores: los códigos de `// expect:` y ningún `.asm` (el runner deja uno viejo antes y verifica que se borre). |

**MARS** (opcional en desarrollo, obligatorio en `test-mips-estricto`): el
runner y la IDE buscan el `.jar` en `MARS_JAR`, en `tests/mips/mars.path`
(ruta local, no se versiona) o en `.deps/Mars4_5.jar`
([descarga](https://github.com/dpetersanderson/MARS/releases)). Java: el
del `PATH` o el de `.deps/jre17`. Antes de usarlo, el runner comprueba que
el `.jar` sea un MARS válido ejecutando un programa mínimo.

**Validación de entrega:** `make validar-entrega` corre toda la batería en
modo estricto y después `tests/validar_rubrica.py`, que cruza el plan de
pruebas (`tests/mips/casos_rubrica.json`: E01–E18, N01–N16 y la prueba
estrella A–E) con los resultados de esa misma corrida y responde
`PASS`/`FAIL`/`BLOCKED` por caso y por apartado del enunciado (llamadas,
registros/pila, MIPS funcional). No asigna puntaje. Los resultados quedan en
[`tests/results/`](tests/results/README.md).

Cómo se ejecuta y se juzga cada programa está en
[`tests/mips/mars.py`](tests/mips/mars.py): un caso pasa solo si el
programa **termina con un syscall de salida**, con la salida y el código
de salida esperados. Un código 0 no alcanza: MARS también sale con 0 al
agotar el límite de pasos (`MARS_MAX_PASOS`, 5 000 000) o si el programa se
cae al final del código. Hay límite de tiempo (`MARS_TIMEOUT`, 30 s). Cada
corrida deja resultados por caso en `tests/results/mips_integracion.json` y
`tests/results/mips_e2e.json`.

**Agregar un caso:**

- Semántico fallido: crear `tests/fixtures/invalid/<nombre>.cps` con
  `// expect: SEM0xx` en las primeras líneas.
- TAC: crear `tests/tac/<nombre>.cps`, correr `make test-tac-update`,
  **revisar** el `.tac` generado (`git diff tests/tac`) y commitear ambos.
  Si una prueba falla por un bug del generador, se arregla el generador, no
  el `.tac` esperado.
- Después de cambiar casos de `tests/tac/`, regenerar los ejemplos de la
  documentación con `make docs-ejemplos`.

---

## 5. IDE

```bash
python3 ide/ide_app.py
```

En Windows se corre desde WSL (con WSLg la ventana abre sola). Antes hay
que haber corrido `bash tools/setup.sh` una vez.

| Acción | Cómo |
|---|---|
| Abrir / escribir un `.cps` | *Archivo → Cargar archivo .cps…* (Ctrl+O), arrastrar el archivo a la ventana, o *Archivo → Ejemplos* (un ejemplo por construcción, de `tests/tac/`). |
| Compilar el compilador | **Compilar** (F5): corre `make build`. |
| Compilar el programa | **Ejecutar** (F6): guarda y corre `./compiscript` sobre el archivo. Si el editor no tiene archivo, compila lo que está escrito. |
| Ver errores | Pestañas *Diagnósticos* (doble clic salta a la línea) y *Errores*. |
| Ver AST y tabla de símbolos | Grupo *Análisis*: texto, y *Tabla de símbolos (columnas)* con dirección/etiqueta, tamaño y frame. |
| Ver el TAC | Grupo *Código intermedio*. Si hubo errores, avisa que no se generó. |
| Guardar el TAC | **Guardar TAC** (toolbar, o *Archivo → Guardar TAC…*). |
| Ver el ensamblador MIPS | Grupo *MIPS* → *Ensamblador*. Si hubo errores (también `CGxxx` del backend), avisa que no se generó y no muestra uno viejo. |
| Guardar el `.asm` | **Guardar ASM…** (grupo *MIPS*, o *Archivo → Guardar ASM (MIPS)…*). |
| Ejecutar en MARS | **Ejecutar en MARS** (F8, opcional): corre el `.asm` en otro hilo, con límite de tiempo (`MARS_TIMEOUT`, 10 s por defecto), y muestra la salida y el código de salida en *Salida MARS*. **Mars.jar…** elige el `.jar`. |
| Correr la batería | **Pruebas** (F7): corre `make test` y muestra arriba un resumen `PASS / FAIL / SKIP` total y por suite (los `SKIP` se cuentan aparte, no como aprobados). |

---

## 6. Estructura del repositorio

```
├── grammar/Compiscript.g4        # gramática oficial, sin modificar
├── generated/                    # salida de ANTLR (gitignored, la regenera make)
├── src/
│   ├── main.cpp                  # CLI: modo completo y modo --tac
│   ├── frontend/                 # ÚNICO lugar que incluye ANTLR: source → AST
│   ├── ast/                      # nodos del AST e impresión (texto y DOT)
│   ├── diagnostics/              # Diagnostic, códigos SYN/SEM (codes.h), reporter
│   ├── compiler/                 # fachada: Compiler::compile(source) → CompilationResult
│   ├── semantic/                 # símbolos, scopes, tipos y los 6 pases semánticos
│   ├── codegen/                  # FrameLayout, TempAllocator y TacGenerator (gen_*.cpp)
│   └── ir/                       # estructuras del TAC (tac.h) e impresión
├── tests/
│   ├── run_tests.py              # runner de toda la batería
│   ├── fixtures/{valid,invalid}/ # casos semánticos
│   ├── tac/                      # casos de TAC (.cps + .tac) y tac/errores/
│   └── symbol_table_test.cpp
├── ide/ide_app.py                # IDE (Tkinter)
├── docs/                         # especificación, arquitectura y ejemplos
├── tools/setup.sh                # instala el toolchain en .deps/, sin sudo
└── Makefile
```

---

## 7. Decisiones de lenguaje

Verificadas contra `Compiscript.g4` y vigentes en esta entrega:

1. **No existe `float`.** Los tipos son `integer`, `string`, `boolean`,
   clases y listas (`integer[]`, `integer[][]`…).
2. **Todo parámetro lleva anotación de tipo** (`SEM011` si falta), aunque
   la gramática la deja opcional. El tipo de retorno sí es opcional: sin
   él, la función es `void`.
3. **El parámetro del `catch` es `string`** (el mensaje del error).
4. **`switch` no exige booleano**: cada `case` debe ser del mismo tipo que
   el sujeto. No hay *fallthrough*: cada `case` termina al final de su
   cuerpo (por eso no hace falta `break`).
5. **`break`/`continue` solo dentro de bucles**: dentro de un `switch` que
   no está en un bucle son `SEM006`.
6. **`string + integer` no está permitido** (`SEM004`): `+` exige dos
   `integer` o dos `string`.
7. **Una subclase se puede usar donde se espera su base**: asignar, pasar
   como argumento y devolver (`let a: Animal = new Perro()`), pero no al
   revés. Los métodos redefinidos se despachan por la vtable del objeto
   (`a.hablar()` corre `Perro_hablar`).
8. **No existe `throw`.** Los errores de tiempo de ejecución (por ejemplo,
   índice fuera de rango) los levanta el runtime y los atrapa `try/catch`.
9. **`if`/`while`/`for`/`foreach` exigen llaves** (`block` en la gramática).
10. **El constructor es una función llamada `constructor`** dentro de la
    clase.
11. **Las funciones, métodos y clases no son valores** (`SEM016`): no se
    puede hacer `let f = doble;`, `let m = obj.metodo;` ni llamar a algo que
    no sea una función.

Los supuestos del TAC (tamaño de palabra, orden de evaluación, globales
estáticas, etc.) están en la sección 2 de
[`docs/08_lenguaje_intermedio.md`](docs/08_lenguaje_intermedio.md).
