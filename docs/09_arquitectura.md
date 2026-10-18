# Arquitectura de la implementación

> Cómo está construido el compilador de Compiscript en esta entrega
> (Proyecto 2: generación de código intermedio). Para el formato del TAC ver
> [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md); para ejemplos
> de traducción ver [`10_ejemplos_de_traduccion.md`](10_ejemplos_de_traduccion.md);
> para instalar y correr, el [`README`](../README.md).

## 1. Vista general

El compilador es un solo ejecutable en C++17 (`compiscript`) organizado como
una cadena de fases. Cada fase recibe lo que produjo la anterior y deja su
resultado en tres estructuras compartidas: el **AST**, la **tabla de
símbolos** y el **reporter de diagnósticos**.

```
            ┌──────────────────────── src/compiler/compiler.cpp ─────────────────────────┐
 .cps ────► │ frontend ──► AST ──► 6 pases semánticos ──► ¿errores? ──no──► codegen ──► TAC│
            │                         │        ▲                │ sí                       │
            │                         ▼        │                ▼                          │
            │                    tabla de símbolos        diagnósticos                     │
            └───────────────────────────────────────────────────────────────────────────┘
                                         │
             src/main.cpp ◄──────────────┘  CompilationResult { ast, diagnostics, symbol_table, tac }
             imprime / exporta output/        ▲
                                              └── ide/ide_app.py (corre el ejecutable y lee su salida)
```

`Compiler::compile(source)` ([`src/compiler/compiler.cpp`](../src/compiler/compiler.cpp))
es la fachada: corre las fases en orden y devuelve un `CompilationResult`
con el AST, los diagnósticos ordenados, la tabla de símbolos y, **solo si no
hubo ningún error**, el programa TAC (`has_tac`). No hay estado global: cada
compilación crea su propio reporter, así la IDE puede compilar muchas veces
en el mismo proceso.

## 2. Fases, en orden

| # | Fase | Archivo(s) | Entrada → salida |
|---|---|---|---|
| 1 | Análisis léxico y sintáctico | `src/frontend/parser_driver.cpp` + parser generado por ANTLR desde `grammar/Compiscript.g4` | texto → parse tree. Los errores de ANTLR pasan por `error_listener.cpp` y se vuelven `SYN001`. |
| 2 | Construcción del AST | `src/frontend/ast_builder.cpp` (Visitor de ANTLR) | parse tree → AST propio (`src/ast/nodes.h`), con línea y columna en cada nodo. |
| 3 | `DeclarationCollector` | `src/semantic/declaration_collector.cpp` | Crea un `Scope` por función, clase y bloque y declara cada símbolo. `SEM002`. |
| 4 | `InheritanceResolver` | `src/semantic/inheritance_resolver.cpp` | Enlaza cada clase con su base. `SEM013` (base inexistente), `SEM014` (ciclo). |
| 5 | `NameResolver` | `src/semantic/name_resolver.cpp` | Resuelve cada identificador usado a su `Symbol`. `SEM001`. |
| 6 | `TypeChecker` | `src/semantic/type_checker.cpp`, `type.cpp` | Calcula el tipo de cada expresión y valida operaciones, asignaciones (una subclase se acepta donde se espera su base), condiciones, llamadas, miembros, `this`, parámetros sin tipo y funciones usadas como valor. `SEM003`–`SEM005`, `SEM008`–`SEM011`, `SEM013`, `SEM015`, `SEM016`. |
| 7 | `ControlFlowChecker` | `src/semantic/control_flow_checker.cpp` | `break`/`continue` fuera de bucle (`SEM006`), `return` fuera de función (`SEM007`), código muerto (`SEM012`). |
| 8 | `ClosureAnalyzer` | `src/semantic/closure_analyzer.cpp` | Anota en cada función anidada qué variables externas captura. No reporta errores. |
| — | **Corte** | `compiler.cpp` | Si `reporter.has_errors()`, se termina aquí: se devuelven los diagnósticos y **no** se genera TAC. |
| 9 | `FrameLayout` | `src/codegen/frame_layout.cpp` | Completa la tabla de símbolos con información de código: almacenamiento, offsets, tamaños, nombres en el TAC, etiquetas, frames, profundidad de anidamiento, tamaño de instancia y la vtable de cada clase. |
| 10 | `TacGenerator` | `src/codegen/tac_generator.cpp` + `gen_expressions.cpp`, `gen_statements.cpp`, `gen_functions.cpp`, `gen_classes.cpp`, `gen_arrays.cpp` | Recorrido dirigido por la sintaxis: cada `gen*` es la acción semántica de un tipo de nodo. AST → `ir::TacProgram`. Usa `TempAllocator` para los temporales. |
| 11 | Impresión | `src/ir/tac_printer.cpp` | `ir::TacProgram` → texto (una instrucción por línea). |

El análisis semántico **no se detiene en el primer error**: todos los pases
corren y reportan lo que encuentran (un tipo `Error` comodín evita errores
en cascada). Así un programa con varios problemas los muestra todos de una
vez.

## 3. Qué hay en cada carpeta

| Carpeta | Responsabilidad | Depende de |
|---|---|---|
| `grammar/` | Gramática oficial de Compiscript, sin modificar. | — |
| `generated/` | Lexer, parser y visitor generados por ANTLR (no se versionan; los crea `make`). | ANTLR |
| `src/frontend/` | **Único lugar que incluye ANTLR.** `parse(source, reporter)` devuelve el AST sin exponer tipos de ANTLR. | `generated/`, `ast/`, `diagnostics/` |
| `src/ast/` | Jerarquía de nodos del AST (un nodo por regla real de la gramática) e impresión en texto y DOT. Cada nodo tiene `symbol` y `resolved_type`, que llenan los pases. | — |
| `src/diagnostics/` | `Diagnostic` (código, severidad, mensaje, línea, columna), el registro de códigos `codes.h` y el `DiagnosticReporter`. | — |
| `src/semantic/` | `Symbol`/`FunctionSymbol`/`ClassSymbol`, `Scope`, `SymbolTable`, el sistema de tipos y los 6 pases. | `ast/`, `diagnostics/` |
| `src/codegen/` | `FrameLayout` (registro de activación y layout de objetos), `TempAllocator` (temporales) y `TacGenerator`. | `ast/`, `semantic/`, `ir/` |
| `src/ir/` | Estructuras del TAC (`Operand`, `Instruction`, `TacFunction`, `TacProgram`) y su impresión. Es el contrato con la fase siguiente (MIPS). | — |
| `src/compiler/` | `Compiler::compile()` y `CompilationResult`. | todo lo anterior |
| `src/main.cpp` | CLI: modo completo (`compiscript archivo.cps`) y modo pruebas (`--tac`). Escribe `output/ast.dot` y `output/programa.tac`. | `compiler/` |
| `ide/` | IDE en Tkinter. No reimplementa nada: corre `make build`, `./compiscript` y `make test` y reparte su salida en pestañas. | el ejecutable |
| `tests/` | Batería de pruebas (ver README §4). | el ejecutable |

## 4. La tabla de símbolos a lo largo de las fases

El enunciado pide que la tabla de símbolos "interactúe con cada fase". Es
una sola estructura (`SymbolTable`, dueña del scope global; cada `Scope`
tiene padre e hijos) que cada fase lee y completa:

| Fase | Qué lee | Qué escribe |
|---|---|---|
| `DeclarationCollector` | — | **La crea**: un `Scope` por función, clase y bloque; un `Symbol` por variable, constante, parámetro, función y clase, con su tipo declarado y línea. |
| `InheritanceResolver` | `ClassSymbol::base_class_name` | `ClassSymbol::base_class` (puntero a la clase base). |
| `NameResolver` | `Scope::resolve` (busca hacia afuera) | `AstNode::symbol` en cada uso de un nombre: el AST queda enlazado a la tabla. |
| `TypeChecker` | tipos declarados, firmas, miembros (subiendo por `base_class`) | `Symbol::resolved_type` y `AstNode::resolved_type`. |
| `ControlFlowChecker` | `Scope::owner` (¿estoy dentro de una función?) | — |
| `ClosureAnalyzer` | a qué función pertenece cada símbolo usado | `FunctionSymbol::captured`. |
| `FrameLayout` | toda la tabla | **Información para código**: `storage` (Global/Param/Local/Field), `offset`, `size`, `ir_name`, `owner_function`; en funciones `label`, `frame_size`, `nesting_depth`, `is_method`, `vtable_slot`, `overridden`; en clases `instance_size`, `vtable_label`, `vtable`. |
| `TacGenerator` | `storage`, `ir_name`, `offset`, `label`, `nesting_depth`, `instance_size`, `captured`, `vtable` | — (solo consulta) |
| Impresión (`main.cpp`) | toda la tabla | Se muestra en la salida con la información agregada; la IDE la muestra también en columnas. |
| Fase de MIPS (siguiente) | `offset`, `frame_size`, `label` | Traducirá cada variable a `offset($fp)` o a una etiqueta global. |

La lista completa de campos y un ejemplo de la tabla impresa están en
[`08_lenguaje_intermedio.md` §7](08_lenguaje_intermedio.md#7-tabla-de-símbolos-extendida).

## 5. Entornos de ejecución: registro de activación

`FrameLayout` fija el registro de activación de cada función (offsets
relativos a `fp`, palabras de 4 bytes):

```
fp + 8 + 4*i   parámetro i        ('this' es el parámetro 0 en métodos)
fp + 4         enlace de acceso   (frame de la función que contiene a esta)
fp + 0         enlace de control  (fp del llamador)
fp - 4         dirección de retorno
fp - 8 - 4*k   variable local k
...            temporales
```

- **Valor de retorno:** en un registro (`$v0` en la fase de MIPS), no en el
  frame.
- **Globales:** toda variable declarada fuera de una función es estática
  (`global g_x, 4`); `main` no tiene frame.
- **Closures:** una función anidada recibe el enlace de acceso (`link fp`
  antes del `call`) y llega a las variables de la función que la contiene
  subiendo `nesting_depth` enlaces (`t0 = fp[4]`, luego el offset de la
  variable). La lista `captured` dice qué variables necesita.
- **Objetos:** `alloc` reserva `instance_size` bytes. La palabra 0 apunta
  a la vtable de la clase real del objeto; después cada atributo está en un
  offset fijo, con los heredados primero. Un método que alguna subclase
  redefine se llama por la vtable (`t = obj[0]`, `t = t[4*slot]`,
  `call t, n`), así `let a: Animal = new Perro()` ejecuta los métodos de
  `Perro`.

Detalle y ejemplos: [`08_lenguaje_intermedio.md` §6 y §11](08_lenguaje_intermedio.md#6-registro-de-activación).

## 6. Temporales

`TempAllocator` (`src/codegen/temp_allocator.*`) entrega siempre el
temporal libre de número más bajo, y quien consume un operando lo libera de
inmediato. Por eso el resultado de una operación reusa el temporal de un
operando (`t0 = t0 + t1`) y una expresión usa tantos temporales como
valores intermedios vivos a la vez. Hay un allocator por función y cada
`func` declara cuántos usó (`temps=N`). Ver
[`08_lenguaje_intermedio.md` §8](08_lenguaje_intermedio.md#8-temporales-asignación-y-reciclaje)
y el caso de prueba `tests/tac/p1_temporales.cps` (10 operaciones, 2
temporales).

## 7. Diagnósticos

Cada error lleva un código estable (`SYN001`, `SEM001`–`SEM016`, ver
[`src/diagnostics/codes.h`](../src/diagnostics/codes.h)), severidad, línea
y columna:

```
[SEM003] error 3:1: el valor asignado no coincide con el tipo de 'x'.
```

Las pruebas comparan **códigos**, no mensajes, para que cambiar la redacción
no rompa la batería. El código de salida del ejecutable es `1` si hubo algún
error.

## 8. IDE y pruebas

- **IDE** (`ide/ide_app.py`): ejecuta el compilador como proceso externo y
  separa su salida por las líneas de sección (`Diagnosticos:`, `AST:`,
  `Tabla de simbolos:`, `Codigo intermedio:`) y los pies `[AST-DOT]` y
  `[TAC]`. Así el compilador y la IDE solo comparten el formato de texto.
- **Pruebas** (`tests/run_tests.py`): corre el ejecutable sobre cada caso y
  compara el código de salida, los códigos de diagnóstico (`// expect:`) y
  el TAC contra archivos esperados. `make test` corre además la prueba
  unitaria de la tabla de símbolos.

## 9. Cómo extender

- **Nueva regla semántica:** reservar el código en `codes.h`, reportarlo
  desde el pase que corresponda y agregar un caso válido y uno fallido en
  `tests/fixtures/`.
- **Nueva construcción en el TAC:** su `gen*` en el `gen_*.cpp` que
  corresponda, la instrucción en `ir/tac.h` y `tac_printer.cpp` si hace
  falta una nueva, su sección en `docs/08`, y un caso en `tests/tac/` con su
  `.tac` (`make test-tac-update` y revisar el diff).
