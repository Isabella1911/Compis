# Compiscript — Proyecto 2

Analizador semántico de **Compiscript** (subconjunto de TypeScript) para el
curso *Construcción de Compiladores*, UVG. Usa **ANTLR** para la fase
sintáctica porque así lo exige el enunciado oficial ("se debe implementar
un analizador sintáctico a partir de la gramática oficial en ANTLR...
recorriéndolo mediante Listeners o Visitors de ANTLR").

## Historial

Este repositorio empezó como `Compis`, un compilador didáctico con lexer y
parser (LL(1)/LR(0)/SLR(1)/LALR(1)) hechos a mano, del curso *Diseño de
Lenguajes*. Ese trabajo quedó preservado íntegro en la rama
[`Deprecated`](../../tree/Deprecated) — no es una continuación de ese
código, sino un proyecto nuevo (stack distinto: ANTLR + C++ generado) que
reemplazó el contenido de `main`.

---

## 1. Pipeline actual

```text
.cps → ANTLR → AstBuilder → AST propio
     → DeclarationCollector → InheritanceResolver → NameResolver
     → TypeChecker: preparar firmas y tipos → validar cuerpos y expresiones
     → ControlFlowChecker → ClosureAnalyzer
     → diagnósticos, tabla de símbolos y AST visualizable
```

La fachada `Compiler::compile()` devuelve `{ ast, diagnostics, symbol_table }`.
La IDE utiliza el mismo ejecutable y muestra sus resultados. El proyecto analiza
el programa fuente; no incluye ejecución, generación de código ni runtime.

Los pases semánticos incluyen recuperación segura de redeclaraciones y ciclos,
firmas independientes del orden textual, mutabilidad y destinos de asignación,
invocabilidad, argumentos, retornos, flujo estructural, arreglos, `foreach`,
`catch`, miembros y capturas multinivel.

El detalle de implementación, decisiones y límites está en
[docs/03_passes_semanticos.md](docs/03_passes_semanticos.md).

---

## 2. Cómo compilar y correr

Primera vez en una máquina nueva (instala JRE, `antlr.jar`, `cmake` portátil
y compila el runtime C++ de ANTLR — todo local a `.deps/`, **sin sudo**):

```bash
bash tools/setup.sh
```

Después, desde la raíz del repo:

```bash
make                                            # genera el parser (si hace falta) y compila
make run FILE=tests/fixtures/valid/hello.cps    # corre un archivo .cps
make test                                       # símbolos, semántica, runner y fixtures con códigos esperados
make test-symbols                               # prueba unitaria de Scope/Symbol (no necesita ANTLR)
make test-semantic                              # regresiones sobre AST y capturas (sin ANTLR)
make clean                                      # borra los binarios y el codigo generado
```

`make` regenera el parser solo si `grammar/Compiscript.g4` cambió. El
binario imprime los diagnósticos (si hay), el AST en texto indentado, la
tabla de símbolos (un `Scope` por línea, indentado por anidamiento), y
exporta `output/ast.dot` (renderizable con `dot -Tpng output/ast.dot -o
ast.png`, igual que Compis ya hace con su autómata LR(0)).

Para abrir la IDE:

```bash
python3 ide/ide_app.py
```

Necesita Python con Tkinter; Graphviz (`dot`) permite renderizar el AST y
`tkinterdnd2` es opcional para arrastrar archivos. `Compilar` construye el
compilador mediante make; `Ejecutar` analiza el `.cps` guardado.
Las líneas con diagnóstico de error se resaltan en el editor. Detalle de
uso en [ide/IDE.md](ide/IDE.md). El instalador de dependencias está
preparado para Linux x86-64.

El runner exige los códigos de `tests/fixtures/expected.json`, salida controlada,
posiciones válidas y terminación antes de cinco segundos por fixture. Un crash,
un timeout o un código de error diferente hacen fallar la prueba.
El checklist de entrega (IDE + batería + docs) está en
[docs/04_ide_y_entrega.md](docs/04_ide_y_entrega.md).

---

## 3. Estructura

```
├── grammar/Compiscript.g4       # gramatica oficial, sin modificar
├── generated/                   # salida de ANTLR (gitignored, se regenera con `make`)
├── src/
│   ├── frontend/                 # UNICO lugar que puede incluir ANTLR
│   │   ├── parser_driver.cpp/.h  #   punto de entrada: source -> AST (sin exponer tipos de ANTLR)
│   │   ├── ast_builder.cpp/.h    #   Visitor: Parse Tree -> AST propio
│   │   └── error_listener.cpp/.h #   errores de ANTLR -> Diagnostic (reemplaza el listener por defecto)
│   ├── ast/
│   │   ├── nodes.h               #   jerarquia del AST, un nodo por regla real del .g4
│   │   └── printer.cpp/.h        #   texto indentado + DOT
│   ├── diagnostics/
│   │   ├── diagnostic.h          #   struct Diagnostic (severity, codigo, mensaje, linea, columna)
│   │   ├── codes.h               #   registro de codigos (SYN0xx, SEM0xx)
│   │   └── reporter.cpp/.h       #   acumula diagnosticos, sin estado global
│   ├── compiler/
│   │   ├── result.h              #   CompilationResult
│   │   └── compiler.cpp/.h       #   fachada publica: Compiler::compile(source)
│   └── semantic/                 # Símbolos, tipos, herencia, flujo y capturas
│       ├── symbol.h                    #   Symbol, FunctionSymbol, ClassSymbol
│       ├── scope.h/.cpp                #   Scope: declare/resolve/resolveLocal, pila de tablas
│       ├── symbol_table.h              #   SymbolTable: dueno del scope global
│       ├── declaration_collector.h/.cpp #  Pass 1: AST -> puebla la tabla de simbolos
│       ├── name_resolver.h/.cpp        #   Pass 2: resuelve identificadores usados, SEM001
│       ├── type.h/.cpp, type_checker.h/.cpp # tipos, preparación de firmas y validación
│       ├── inheritance_resolver.h/.cpp  #   enlaces de herencia y ciclos
│       ├── control_flow_checker.h/.cpp  #   contexto, retornos y código muerto
│       ├── closure_analyzer.h/.cpp      #   capturas y receptor léxico
│       └── printer.h/.cpp              #   texto indentado del arbol de scopes
├── tests/
│   ├── fixtures/{valid,invalid}/
│   ├── symbol_table_test.cpp     #   prueba unitaria de Scope/Symbol
│   ├── semantic_test.cpp         #   regresiones sobre AST y metadatos
│   ├── run_fixtures.py           #   códigos exactos, salida y timeout
│   └── fixture_runner_test.py    #   pruebas del runner
├── tools/setup.sh                # instala el toolchain (JRE+antlr+cmake+runtime), sin sudo
└── Makefile
```

**Regla verificable**: fuera de `src/frontend/`, nada debería incluir
headers de ANTLR. `compiler.cpp` solo conoce `frontend/parser_driver.h`, que
no expone ningún tipo de ANTLR en su firma.

---

## 4. Decisiones de lenguaje ya cerradas (verificadas línea por línea contra `Compiscript.g4`, no asumidas)

1. **No existe `float`.** `baseType` es exactamente `'boolean' | 'integer' |
   'string' | Identifier`. No se agrega salvo que el profesor lo exija.
2. **Los parámetros requieren anotación semánticamente.** Aunque la gramática
   permite omitirla, se emite `SEM011` y se recupera con tipo `Error`. Omitir el
   retorno de una función significa `Void` interno. No hay inferencia de parámetros.
3. **El parámetro del `catch` no lleva tipo en la gramática**
   (`'catch' '(' Identifier ')'`). Se fija `string` por decisión de equipo.
4. **La implementación conserva sujetos de `switch` comparables con sus casos**
   (`SEM004` para incompatibilidad). El PDF exige un sujeto booleano: esta
   discrepancia requiere aclaración para la entrega; no se cambió la política
   existente durante la corrección de pases.
5. **`switch` no cuenta como bucle**: `break`/`continue` requieren un bucle
   contenedor (`SEM006`), y una función anidada reinicia ese contexto.
6. **`string + integer` no se permite.** `+` acepta dos enteros o dos strings.
7. **No existe `throw`.** `try/catch` se valida sintáctica y
   semánticamente, pero no hay sentencia para lanzar errores propios.
8. **`if`/`while`/`do-while`/`for`/`foreach` exigen bloque con llaves**,
   nunca una sentencia suelta (`ifStatement: ... block ('else' block)?`).
   Esto **contradice los ejemplos de `Especificaciones.md`**
   (`if (n < 60) continue;` sin llaves) — se descubrió al correr el fixture
   `tests/fixtures/valid/completo.cps`: sin llaves, ANTLR rechaza con
   `missing '{' at 'continue'`. Hay que escribir Compiscript real siempre
   con llaves, o pedirle al profesor que aclare/corrija el ejemplo.
9. **El constructor no tiene palabra reservada**: es un
   `FunctionDeclaration` cuyo `name` es literalmente `"constructor"`.
10. **Quirk de la gramática (no de esta implementación): `Literal`,
    `IntegerLiteral` y `StringLiteral` son tres reglas de lexer
    distintas, pero por el orden de declaración el lexer real SIEMPRE
    emite el token `Literal`** (nunca los otros dos por separado,
    verificado corriendo el lexer generado sobre `42` y `"hola"`). Por eso
    `AstBuilder::visitLiteralExpr` distingue entero de string mirando el
    texto (si empieza con `"`), no el tipo de token.

---

## 5. Estado (checklist)

### Etapa 1 — Fundación (AST + diagnósticos + fachada)

- [x] Toolchain de ANTLR instalado sin sudo (`tools/setup.sh`), reproducible en cualquier máquina del equipo.
- [x] Parser generado desde `grammar/Compiscript.g4` sin tocar la gramática oficial.
- [x] Un nodo de AST por cada regla real de la gramática (incluidos ternario, `print`, asignación como expresión, `PropertyAssignment`).
- [x] Operadores binarios plegados a la izquierda (`a - b - c` -> `((a-b)-c)`).
- [x] `leftHandSide` con sufijos encadenados a mano (`a.b[0].c()`).
- [x] Todo nodo tiene `line`/`column`.
- [x] `DiagnosticErrorListener` propio (nada de errores sintácticos por stderr).
- [x] `Compiler::compile()` sin estado global entre llamadas.
- [x] AST exportable a texto y a DOT.
- [x] Batería mínima de pruebas (`make test`) con casos válidos e inválidos.

### Etapa 2 — Símbolos y pases semánticos

- [x] **Tabla de símbolos** (`src/semantic/symbol.h`, `scope.h/.cpp`, `symbol_table.h`):
      `Symbol`/`FunctionSymbol`/`ClassSymbol`, `Scope` (declare/resolve/resolveLocal,
      pila de tablas con shadowing correcto), `SymbolTable` como dueño del scope
      global. Probada en aislado (`make test-symbols`, 24 checks).
- [x] **DeclarationCollector** (`declaration_collector.h/.cpp`): recorre el AST,
      crea un `Scope` por cada función/clase/bloque (el cuerpo inmediato de una
      función o clase comparte el scope de sus parámetros/miembros, sin anidar
      de más), declara cada variable/constante/parámetro/función/clase, y
      reporta `SEM002` en redeclaraciones dentro del mismo ámbito. **Ya
      conectado a `Compiler::compile()`** — `CompilationResult` ahora expone
      `symbol_table` siempre válido (scope global vacío si hubo error
      sintáctico). Visible con `make run FILE=...` (imprime la tabla de
      símbolos completa) y probado con fixtures reales, incluida una
      redeclaración real que dispara `SEM002` de punta a punta.
- [x] **NameResolver** (`name_resolver.h/.cpp`): recorre el AST entrando a las
      expresiones (no solo las declaraciones), resuelve cada identificador
      usado contra la tabla de símbolos (llena `AstNode::symbol` en
      `IdentifierExpression`, `NewExpression`, y en el target de
      `AssignmentStatement`), y reporta `SEM001` si no existe. Delega los miembros (`obj.campo`),
      el contexto de `this` y las llamadas a `TypeChecker`. **Ya conectado** —
      corre justo después de `DeclarationCollector` en `Compiler::compile()`.
      Probado: `SEM001` dispara con una variable no declarada, y
      `completo.cps` (clases, herencia, recursión, `this`, cadenas
      `a.b[0].c()`) sigue sin falsos positivos.
- [x] **Sistema de tipos** (`type.h/.cpp`): clase `Type` (con `equals()` estructural
      y `TypeKind::Error` como comodín que siempre es compatible, para no generar
      cascadas de errores), `resolveTypeAnnotation()` para convertir un
      `TypeAnnotation` en un `Type` real (resolviendo nombres de clase contra la
      tabla de símbolos, `SEM013` si no es válido). `Symbol` ahora tiene
      `resolved_type`; `Scope` tiene `owner` (el `Symbol` dueño del scope, para
      poder responder "¿en qué función/clase estoy parado?"). Ver
      [la guía técnica](docs/03_passes_semanticos.md) para el diseño completo.
- [x] **TypeChecker** (Pass 3, `type_checker.h/.cpp`): llena `resolved_type` en
      todo el árbol y valida tipos en operaciones aritméticas/lógicas/comparaciones
      (`SEM004`), asignaciones (`SEM003`), condiciones de `if`/`while`/`do-while`/`for`/
      ternario (`SEM005`), tipo de retorno (`SEM009`), compatibilidad `switch`/`case`,
      tipos de elementos de arreglo e índices. **Ya conectado**, corre después de
      `NameResolver`. Prepara todas las firmas antes de revisar los cuerpos y resuelve
      miembros y argumentos sin depender del orden de declaración. Probado con
      fixtures dedicados por código (`sem003_*`, `sem004_*`, `sem005_*`, `sem013_*`)
      más `tipos.cps` (funciones, arreglos, `for`, `switch`, ternario) sin falsos positivos.
- [x] **InheritanceResolver** (`inheritance_resolver.h/.cpp`): resuelve
      `ClassSymbol::base_class_name` al `ClassSymbol* base_class` real (funciona
      con clases declaradas en cualquier orden), y detecta herencia circular
      (`SEM014`). Corre entre `DeclarationCollector` y `NameResolver`. Probado:
      `herencia.cps` (cadena de 3 niveles, referencia hacia adelante) sin
      diagnósticos; base inexistente → `SEM013`; ciclo → `SEM014`.
- [x] **Clases y Objetos, y validación de argumentos** (extendido dentro de
      `TypeChecker`, no un pass nuevo): atributos/métodos heredados vía `.`
      (`lookupMember()`, busca en la propia clase y sube por `base_class`,
      `SEM010` si no existe), invocación del constructor (mismo mecanismo,
      busca `"constructor"`), `this` tipado como la clase contenedora y
      fuera de contexto (`SEM015`), y número/tipo de argumentos en
      llamadas a funciones, métodos y constructor (`SEM008`). Probado con
      `clases_y_objetos.cps` (método y campo heredados a través de una
      subclase, `this`, constructor y método con argumentos correctos) sin
      diagnósticos, más fixtures dedicados para cada código nuevo.
- [x] **ControlFlowChecker** (`control_flow_checker.h/.cpp`): `break`/`continue`
      fuera de un bucle (`SEM006`; `switch` no cuenta como bucle, decisión
      literal del PDF ya tomada), `return` fuera de una función (`SEM007`),
      código muerto estructural (`SEM012`, una vez por lista) y caminos que
      alcanzan el final de funciones con retorno (`SEM020`). Pass independiente.
- [x] **ClosureAnalyzer** (`closure_analyzer.h/.cpp`): identifica qué
      variables no globales necesita cada función anidada y las guarda en
      `FunctionSymbol::captured` (no valida nada — información para
      generación de código futura). Propaga capturas por funciones intermedias
      y registra `captured_this` por separado. Visible en `printScopeTree()` como
      `[captura: ...]` junto a la firma de la función.
      Probado: `sem006_*`, `sem007_*`, `sem012_*` disparan su código exacto;
      `control_flow_y_closures.cps` (break/continue/return correctos, una
      función anidada capturando el parámetro de su contenedora) sin
      diagnósticos y con la captura visible en la tabla de símbolos.

- [x] Recuperación segura de símbolos rechazados y búsquedas de herencia con
      clases visitadas; parámetros sin tipo reportados con `SEM011`.
- [x] Asignaciones verifican destino y mutabilidad; `new`, llamadas, `foreach`
      y tipos no inferibles tienen diagnósticos explícitos (`SEM016`–`SEM019`).
- [x] `catch` tiene tipo resuelto `string`; arreglos vacíos usan un marcador
      contextual propio, sin ocultar errores con `TypeKind::Error`.
- [x] IDE conectada al frontend actual y batería ampliada por regla, incluyendo
      códigos esperados, recuperación, orden de firmas y metadatos de capturas.
- [x] Resaltado de líneas con error en la IDE y documentación de entrega
      ([ide/IDE.md](ide/IDE.md), [docs/04_ide_y_entrega.md](docs/04_ide_y_entrega.md)).

La guía [03_passes_semanticos.md](docs/03_passes_semanticos.md) documenta las
políticas de `this`, constantes, retornos y capturas, y las discrepancias del PDF
sobre `float` y `switch`. No se añaden subtipado ni contratos de override porque
el enunciado proporcionado no especifica esas reglas.

### Fuera de Proyecto 2

Generación de código, IR, ejecución de programas y runtime, incluido el garbage
collector. No se implementan en esta fase.
