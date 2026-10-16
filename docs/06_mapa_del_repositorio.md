# 06 — Mapa del repositorio y guía rápida

> Este documento nace de revisar los 3 commits que subió **Cisco890**
> (`7c396dd`, `4a3b093`, `a2d7929` en `origin/main`, aún no traídos a este
> `main` local) sobre la base que dejamos nosotros (`61dd444`). Todo lo
> descrito aquí fue **verificado**, no solo leído: se compiló el proyecto
> completo y se corrió `make test` en un worktree aparte (sin tocar tu
> checkout) — build limpio, **140/140 casos ok, 0 fallos**.

---

## 1. Qué trae esta subida, en una frase

Cisco890 llevó el análisis semántico de "cubre las reglas del PDF" a
"cubre las reglas del PDF **y se recupera de errores como un compilador
real**": agregó 5 códigos nuevos (SEM016–SEM020), reescribió el
`TypeChecker` para que ya no falle con referencias hacia adelante,
reescribió `ControlFlowChecker` con un análisis de flujo estructural de
verdad (no dos banderas sueltas), separó la captura de `this` de la
captura de variables en `ClosureAnalyzer`, hizo que las declaraciones
duplicadas se conserven para recuperación en vez de perderse, escribió
una batería de **140 fixtures** con verificación de código exacto, y dejó
un scaffold de garbage collector (mark-and-sweep) fuera del alcance de
este proyecto pero listo para cuando toque generación de código.

---

## 2. Mapa de archivos

### `grammar/`

| Archivo | Qué hace |
|---|---|
| `Compiscript.g4` | Gramática oficial de ANTLR4. Sin modificar — es la única fuente de verdad sintáctica. |

### `src/frontend/` — único módulo que puede incluir headers de ANTLR

| Archivo | Qué hace |
|---|---|
| `parser_driver.h/.cpp` | Punto de entrada `source → AST`: arma lexer, parser y el listener de errores de ANTLR. |
| `ast_builder.h/.cpp` | El `Visitor` de ANTLR: recorre el parse tree y construye el AST propio (nodos en `src/ast/`), desacoplado de tipos de ANTLR. |
| `error_listener.h/.cpp` | Listener custom que traduce errores léxicos/sintácticos de ANTLR a un `Diagnostic` (`SYN001`). |

### `src/ast/`

| Archivo | Qué hace |
|---|---|
| `nodes.h` | Jerarquía completa del AST (~50 tipos de nodo, uno por regla real de la gramática). |
| `printer.h/.cpp` | Imprime el AST como texto indentado y como DOT (Graphviz) — cubre el requisito explícito de "representación visual del árbol". |

### `src/diagnostics/`

| Archivo | Qué hace |
|---|---|
| `diagnostic.h` | El struct `Diagnostic` (código, severidad, línea, columna, mensaje). |
| `codes.h` | **El contrato del proyecto.** Registro de todos los códigos: `SYN001`, `SEM001`–`SEM020`. La batería de pruebas compara contra estos códigos exactos, no contra texto. |
| `reporter.h/.cpp` | `DiagnosticReporter`: acumula y ordena los diagnósticos de una corrida. |

### `src/compiler/`

| Archivo | Qué hace |
|---|---|
| `compiler.h/.cpp` | `Compiler::compile(source)` — la fachada única. Encadena los 6 pases semánticos en orden fijo. |
| `result.h` | `CompilationResult { ast, diagnostics, symbol_table }`. |

### `src/semantic/` — el corazón del análisis (60 + 25 pts de la rúbrica)

| Archivo | Qué hace | Cambió con esta subida |
|---|---|---|
| `symbol.h` | `Symbol` / `FunctionSymbol` / `ClassSymbol`. | Sí — nuevos campos `is_rejected`, `declaring_scope`, `declaration`, `captured_this`. |
| `scope.h/.cpp` | Pila de tablas (`Scope` con padre/hijos), `declare/resolve/resolveLocal`. | Sí — los símbolos duplicados ya no se descartan: se guardan en `rejected_symbols_` para que el AST siga siendo válido tras un `SEM002` (recuperación de errores real). |
| `symbol_table.h` | Contenedor raíz: el scope global. | No. |
| `type.h/.cpp` | Sistema de tipos estructural (`Type::equals`, `Error` como comodín). | Sí — nuevo `TypeKind::EmptyElement` (para `[]` sin forzar `Error`) y `commonType()` para unificar tipos de arreglos/ternarios. |
| `declaration_collector.h/.cpp` | Pase 1: construye scopes, declara símbolos, reporta `SEM002`. | Menor — guarda `declaration` en cada símbolo para soportar inferencia de tipo. |
| `inheritance_resolver.h/.cpp` | Resuelve la cadena de herencia (`SEM013`/`SEM014`). | No. |
| `name_resolver.h/.cpp` | Pase 2: resuelve identificadores contra el scope (`SEM001`). | No (solo un comentario). |
| **`type_checker.h/.cpp`** (624 líneas — el archivo más grande del proyecto) | Pase 3: llena tipos, valida asignaciones/operaciones/condiciones/retornos/miembros/`this`/argumentos. | **Sí, reescrito.** Ahora hace una fase previa (`prepareScope`/`prepareSymbol`) que resuelve firmas y tipos declarados de TODO el árbol antes de revisar ningún cuerpo — esto elimina la limitación de "referencia hacia adelante" que documentamos la sesión pasada (una función llamada antes de que el pase llegara a su propia declaración quedaba sin validar). También suma `SEM016`–`SEM019`. |
| **`control_flow_checker.h/.cpp`** | Pase 4: `break`/`continue` fuera de bucle, `return` fuera de función, código muerto. | **Sí, reescrito.** Antes llevaba dos banderas booleanas (`insideLoop`/`insideFunction`); ahora calcula un resumen de flujo por bitmask (`FallsThrough / Returns / Breaks / Continues`) por statement y por bloque completo — es lo que hace posible el nuevo `SEM020` (función con tipo de retorno que puede terminar sin devolver valor en algún camino) y mejora la detección de código muerto. |
| **`closure_analyzer.h/.cpp`** | Pase 5: no valida nada, anota qué variables captura cada función anidada (metadata para generación de código futura). | **Sí, reescrito.** Separa la captura de variables (`captured`) de la captura del receptor léxico `this` (`captured_this`, nuevo) y corrige un detalle real: antes de este cambio, funciones/clases/globales podían colarse como "capturadas"; ahora se excluyen correctamente. |
| `printer.h/.cpp` | Imprime el árbol de scopes como texto (la salida "Tabla de símbolos" del CLI). | Menor — ahora también muestra `[captura this: NombreClase]`. |

### `src/runtime/` — **fuera de la rúbrica de Proyecto 2**, prep para código futuro

| Archivo | Qué hace |
|---|---|
| `type_descriptor.h` | `SlotKind` (Primitive/Pointer), `FieldSlot`, `ClassDescriptor`, `ArrayDescriptor` — describen el layout de memoria de una clase/arreglo. |
| `heap.h/.cpp` | Único lugar que reserva memoria dinámica del runtime (`HeapObject`, `ObjectHeader`). Nada más debería llamar `malloc` directo. |
| `gc.h/.cpp` | Mark-and-sweep clásico: raíces explícitas, pila propia en el mark (no usa el stack de C++), umbral configurable antes de recolectar. |
| `descriptor_builder.h/.cpp` | Construye los descriptores de arriba a partir de `ClassSymbol` y `FunctionSymbol::captured` ya resueltos — sin recalcular nada semántico. |

Todo este módulo es un **scaffold deliberado**: no lo pide el enunciado
de este proyecto, pero deja lista la infraestructura de memoria para
cuando el curso llegue a generación de código.

### `ide/`

| Archivo | Qué hace |
|---|---|
| `ide_app.py` | La IDE Tkinter (la que adaptamos la sesión pasada al compilador ANTLR nuevo). Cisco890 le sumó resaltado de líneas con error directamente en el editor (fondo + número en el gutter en rojo), sincronizado con la tabla de diagnósticos. |
| `IDE.md` | **Nuevo.** Guía de uso de la IDE: requisitos, atajos, qué hace cada pestaña, y el contrato exacto de stdout que la IDE espera del CLI (para que nadie rompa la IDE sin querer al tocar un pase semántico). |

### `tests/`

| Archivo | Qué hace |
|---|---|
| `symbol_table_test.cpp` | Unitarios de `Scope`/`Symbol`, sin ANTLR (compila rápido). |
| `semantic_test.cpp` | **Nuevo.** Regresiones sobre metadatos del AST (capturas, recuperación de símbolos rechazados) construyendo el AST a mano en C++, sin pasar por ANTLR. |
| `runtime_gc_test.cpp` | **Nuevo.** Prueba heap + mark-and-sweep (incluye ciclos) + descriptores. |
| `run_fixtures.py` | **Nuevo — el corazón de la batería.** Corre el binario contra cada fixture y exige el **conjunto exacto** de códigos definido en `expected.json`; también falla si el exit code no cuadra, si hay `stderr` inesperado, si una posición línea/columna es inválida, o si se pasa de un timeout de 5s. |
| `fixture_runner_test.py` | **Nuevo.** Meta-tests: prueba que `run_fixtures.py` no confunda un crash con un rechazo válido (un caso sutil y fácil de pasar por alto). |
| `fixtures/valid/` | 30 programas `.cps` que deben compilar sin diagnósticos. |
| `fixtures/invalid/` | 110 programas `.cps`, nombrados por regla (`sem004_...`, `sem016_...`), cada uno pensado para disparar un código específico. |
| `fixtures/expected.json` | **Nuevo.** Mapa exacto `nombre_de_fixture.cps → [códigos esperados]`. Es lo que convierte "hubo un error" en "hubo *el* error correcto". |

### `tools/`

| Archivo | Qué hace |
|---|---|
| `setup.sh` | Instala JRE, `antlr.jar`, CMake portátil y compila el runtime C++ de ANTLR, todo dentro de `.deps/`, sin `sudo`. Se corre una sola vez por máquina. |

### Raíz del repo

| Archivo | Qué hace |
|---|---|
| `Makefile` | Reescrito: además de `build`/`run`/`test-symbols`, ahora tiene `test-semantic` y `test-runtime`; `make test` corre **todo** (symbols + semantic + runtime + los dos runners de fixtures en Python). |
| `README.md` | Reescrito casi por completo — ahora es el índice operativo real del proyecto (pipeline, estructura, tabla de símbolos, códigos, mapeo regla↔código, decisiones de lenguaje, IDE, batería, rúbrica). |
| `docs/proyecto_compis.pdf` | **Nuevo.** El informe formal del curso (carátula con los 3 integrantes, descripción, objetivos, mismo mapeo regla↔código que el README). |
| `.ide_state.json` | Estado persistido de la IDE (último `.cps` abierto, geometría de ventana). Se sobreescribe solo; no hace falta tocarlo. |

---

## 3. Puntos especiales — qué archivos pesan más según la rúbrica

La rúbrica del curso reparte 100 pts así: **Analizador Sintáctico y
Semántico (60)** · **Tabla de símbolos (25)** · **IDE (15)**. En orden de
qué archivo mover primero si algo falla o si hay que explicar el
proyecto en la entrega:

1. **`src/diagnostics/codes.h` + `tests/fixtures/expected.json`** — son
   el contrato. Si un código cambia de significado o un fixture cambia de
   código esperado sin actualizar el otro lado, `make test` lo va a
   agarrar (ya lo comprobé: los 140 casos están sincronizados ahora
   mismo).
2. **`src/semantic/type_checker.cpp`** — el pase que más reglas del PDF
   cubre por sí solo (tipos, asignaciones, condiciones, retorno,
   miembros, `this`, argumentos, invocabilidad, `foreach`, inferencia).
   Es también el más grande y el que más cambió en esta subida.
3. **`src/semantic/control_flow_checker.cpp` y `closure_analyzer.cpp`**
   — ambos reescritos con un diseño más sólido; si algo del control de
   flujo o de las capturas se comporta raro, empezar acá.
4. **`src/semantic/scope.h/.cpp` y `symbol.h`** — son los 25 pts de
   tabla de símbolos. El mecanismo de recuperación (`rejected_symbols_`)
   es nuevo y vale la pena poder explicarlo en la entrega: una
   redeclaración ya no rompe el resto del análisis.
5. **`ide/ide_app.py` + `ide/IDE.md`** — los 15 pts de IDE. `IDE.md`
   documenta el contrato de stdout que la IDE espera del CLI — léanlo
   antes de tocar el formato de salida de `src/main.cpp`, porque ahí es
   donde se puede romper la IDE sin que ningún test C++ lo detecte.
6. **`src/runtime/`** — no suma puntos de este proyecto, pero es
   trabajo real de cara al siguiente. No hace falta tocarlo para la
   entrega actual.

---

## 4. Cosas para resolver con el equipo (encontradas al verificar, no al leer)

- **El nuevo `README.md` enlaza a documentos que no están en el repo:**
  `docs/03_passes_semanticos.md`, `docs/04_ide_y_entrega.md`,
  `docs/05_generacion_de_codigo_y_runtime.md`,
  `docs/ANALISIS_ESTADO_PROYECTO.md` y `docs/informe/informe.tex`
  aparecen citados pero `git ls-tree` confirma que **ninguno existe** en
  `origin/main` — ni siquiera el `.tex` fuente del PDF que sí se subió
  (`docs/proyecto_compis.pdf`, que es el PDF ya compilado, en la raíz de
  `docs/`, no en `docs/informe/` como dice el README). Los primeros tres
  nombres coinciden exactamente con los que nosotros escribimos en
  sesiones anteriores — probablemente Cisco890 los vio localmente (o le
  compartiste el contexto) y asumió que ya estaban commiteados. Vale la
  pena que alguien los suba, o que el README deje de referenciarlos.
- **Nuestros `docs/02_sistema_de_tipos.md` y `docs/03_passes_semanticos.md`
  locales están desactualizados**: no mencionan `SEM016`–`SEM020` ni el
  rediseño de `TypeChecker`/`ControlFlowChecker`/`ClosureAnalyzer`. Si se
  suben tal cual, van a contradecir el README nuevo.
- **`runtime_gc_test` (binario, 2.2MB) quedó commiteado en la raíz del
  repo.** El `.gitignore` sí ignora `compiscript` y `symbol_table_test`,
  pero no a este binario nuevo — es casi seguro un descuido, no algo
  intencional.
- El `Makefile` (target `test`) y el README mencionan `make -C
  docs/informe` para generar el PDF del informe, pero ese target no
  existe en el `Makefile` real ni existe `docs/informe/`.

Nada de esto rompe la nota — el código compila limpio y pasa 140/140 —
pero son detalles de "prolijidad de la entrega" fáciles de arreglar
antes de la fecha límite.

---

## 5. Guía rápida: correr y ver la IDE funcionando

Esto asume Linux/WSL con `g++` (C++17), Python 3.8+ con Tkinter, y
opcionalmente Graphviz (`dot`) para el AST gráfico.

```bash
# 1. Solo la primera vez en una máquina nueva: instala JRE + antlr.jar +
#    CMake + runtime C++ de ANTLR, todo local a .deps/, sin sudo.
bash tools/setup.sh

# 2. Compila el binario `compiscript`.
make

# 3. (Opcional pero recomendado) corre toda la batería para confirmar
#    que tu checkout está sano — debería dar "140 ok, 0 fallos".
make test

# 4. Abre la IDE.
python3 ide/ide_app.py
```

Dentro de la IDE:

1. **Cargá un `.cps`** — desde el panel "Proyecto" a la izquierda podés
   abrir directamente los fixtures de `tests/fixtures/valid/` o
   `tests/fixtures/invalid/`, o escribir el tuyo.
2. **Compilar (`F5`)** — corre `make` y construye el binario si hiciste
   cambios en el código fuente C++.
3. **Ejecutar análisis (`F6`)** — guarda el archivo activo y corre
   `./compiscript archivo.cps`.
4. Revisá el resultado en las pestañas de abajo:
   - **Diagnósticos** — tabla con código/severidad/línea/columna/mensaje.
     Doble clic en una fila salta directo a esa posición en el editor,
     y esa línea queda resaltada en rojo (fondo + número en el gutter).
   - **AST / Tabla de símbolos** — selector que alterna entre el árbol
     sintáctico y el árbol de scopes, ambos en texto plano tal como los
     imprime el CLI.
   - **Visualización → AST (gráfico)** — botón "Renderizar PNG" (necesita
     `dot` de Graphviz en el PATH) para ver el árbol como imagen.

Para probar sin la IDE, directo desde terminal:

```bash
make run FILE=tests/fixtures/valid/hello.cps
make run FILE=tests/fixtures/invalid/sem020_sin_retorno.cps   # dispara SEM020
```

El binario imprime `Diagnosticos:` (si hay), luego `AST:`, luego
`Tabla de simbolos:`, y exporta `output/ast.dot` en cada corrida.
