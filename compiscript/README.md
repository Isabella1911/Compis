# Compiscript — Proyecto 2 (rama `ANTLR`)

Analizador semántico de **Compiscript** (subconjunto de TypeScript) para el
curso *Construcción de Compiladores*. A diferencia de `Compis` (la raíz de
este repo, Proyecto 1 de *Diseño de Lenguajes*, con lexer/parser hechos a
mano), este proyecto usa **ANTLR** para la fase sintáctica porque así lo
exige el enunciado oficial ("se debe implementar un analizador sintáctico a
partir de la gramática oficial en ANTLR... recorriéndolo mediante Listeners
o Visitors de ANTLR"). Vive en una rama aparte porque es un stack distinto
(ANTLR + C++ generado vs. C++ puro), no una continuación de código.

---

## 1. Qué hace esta etapa (Fundación: AST + diagnósticos + fachada)

```
program.cps
    │
    ▼
CompiscriptLexer / CompiscriptParser   (generados por ANTLR desde grammar/Compiscript.g4)
    │
    ▼
Parse Tree de ANTLR
    │
    ▼
AstBuilder (Visitor)                    src/frontend/ast_builder.cpp
    │
    ▼
AST propio                              src/ast/nodes.h
    │
    ├──► printTree() / toDot()          src/ast/printer.cpp
    │
    ▼
Compiler::compile() -> CompilationResult   src/compiler/compiler.cpp
```

Lo que **no** está implementado todavía (Etapas 2 y 3, fuera de este
documento): tabla de símbolos, sistema de tipos, y las validaciones
semánticas reales (todo lo que pide la sección "Especificaciones" del PDF
del proyecto). Los campos `resolved_type`, `symbol` y `scope` ya existen en
cada nodo del AST, en `nullptr`, listos para que esas etapas los llenen sin
tener que volver a tocar `nodes.h`.

---

## 2. Cómo compilar y correr

Primera vez en una máquina nueva (instala JRE, `antlr.jar`, `cmake` portátil
y compila el runtime C++ de ANTLR — todo local a `.deps/`, **sin sudo**):

```bash
bash tools/setup.sh
```

Después, desde `compiscript/`:

```bash
make                                            # genera el parser (si hace falta) y compila
make run FILE=tests/fixtures/valid/hello.cps    # corre un archivo .cps
make test                                       # corre todos los fixtures de tests/fixtures/
make test-symbols                               # prueba unitaria de Scope/Symbol (no necesita ANTLR)
make clean                                      # borra los binarios y el codigo generado
```

`make` regenera el parser solo si `grammar/Compiscript.g4` cambió. El
binario imprime los diagnósticos (si hay), el AST en texto indentado, y
exporta `output/ast.dot` (renderizable con `dot -Tpng output/ast.dot -o
ast.png`, igual que Compis ya hace con su autómata LR(0)).

---

## 3. Estructura

```
compiscript/
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
│   └── semantic/                 # Etapa 2: tabla de simbolos (por ahora), tipos y passes despues
│       ├── symbol.h              #   Symbol, FunctionSymbol, ClassSymbol
│       ├── scope.h/.cpp          #   Scope: declare/resolve/resolveLocal, pila de tablas
│       └── symbol_table.h        #   SymbolTable: dueno del scope global
├── tests/
│   ├── fixtures/{valid,invalid}/
│   └── symbol_table_test.cpp     #   prueba unitaria de Scope/Symbol, sin AST ni compilador
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
2. **Los parámetros y el retorno pueden ir sin anotación de tipo**
   (`parameter: Identifier (':' type)?`). El AST lo refleja con
   `declared_type == nullptr`; la Etapa 3 decide si eso es error (`SEM011`,
   ya reservado en `codes.h`) o si asume un tipo por defecto.
3. **El parámetro del `catch` no lleva tipo en la gramática**
   (`'catch' '(' Identifier ')'`). Se fija `string` por decisión de equipo.
4. **`switch` no exige booleano**: el sujeto y cada `case` son `expression`
   arbitraria. Cada `case` debe ser comparable con el tipo del sujeto (a
   decidir en la Etapa 3).
5. **`break`/`continue` son sintácticamente válidos dentro de un
   `switch`**, no solo dentro de bucles (la gramática los permite en
   cualquier `statement*`, incluido el de `switchCase`). La Etapa 3 decide
   si eso es un error semántico.
6. **`string + integer` — pendiente de decidir.** Recomendado: no
   permitirlo, obliga a conversión explícita.
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

### Etapa 2 — Símbolos y tipos (en progreso, por rebanadas)

- [x] **Tabla de símbolos** (`src/semantic/`): `Symbol`/`FunctionSymbol`/`ClassSymbol`,
      `Scope` (declare/resolve/resolveLocal, pila de tablas con shadowing correcto),
      `SymbolTable` como dueño del scope global. Probada en aislado
      (`make test-symbols`, 24 checks), **todavía no conectada al AST ni al
      compilador** — eso es la siguiente rebanada (un pass que recorra el AST
      y cree/llene los scopes: global, función, clase, bloque).
- [ ] Pass que puebla la tabla de símbolos recorriendo el AST (DeclarationCollector).
- [ ] Resolución de nombres sobre el AST (llenar `AstNode::symbol`/`scope`).
- [ ] Sistema de tipos interno (`Type`, `resolved_type`).
- [ ] Passes semánticos (Etapa 3): verificación de tipos, control de flujo, clases, closures, código muerto — las ~25 reglas del PDF.
- [ ] IDE (por ahora hay CLI vía `make run`; se evaluará adaptar la IDE tkinter de Compis).
