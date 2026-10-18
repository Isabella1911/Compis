# 07 — Archivos lógicos: guía profunda

> **Documento histórico.** Describe la versión del código anterior al merge
> `d1fa312`, que se descartó (incluye archivos como `src/runtime/` que ya no
> existen en `main`). No refleja el estado actual: para eso ver el
> [README](../README.md) y [`09_arquitectura.md`](09_arquitectura.md).


> Alcance de este documento: **solo `src/`** — el código que realmente
> implementa el compilador (frontend ANTLR, AST, diagnósticos, fachada,
> los 6 pases semánticos, y el scaffold de runtime/GC). No repite lo que
> ya cubre [`06_mapa_del_repositorio.md`](06_mapa_del_repositorio.md)
> sobre tests, IDE, docs o config — acá va función por función, con la
> lógica real de cada archivo, leída directamente del código en
> `origin/main` (commit `a2d7929`).

**Orden de lectura recomendado** (sigue el pipeline real, no el
alfabético): `frontend/` → `ast/` → `diagnostics/` → `compiler/` →
`semantic/` (en el orden en que corren los 6 pases) → `runtime/`.

---

## 1. `src/frontend/` — el único módulo que toca ANTLR

Regla de diseño verificable: `grep -r antlr src/ --exclude-dir=frontend`
no debe encontrar nada. Todo lo que sale de acá es el AST propio de
`src/ast/`, sin un solo tipo de ANTLR filtrándose al resto del compilador.

### `parser_driver.h` / `.cpp`

**Propósito:** el único punto de entrada `std::string source → ast::ProgramPtr`.

**Lógica de `parse()`:**
1. Arma `ANTLRInputStream` → `CompiscriptLexer` → `CommonTokenStream` →
   `CompiscriptParser`.
2. Reemplaza los error listeners por defecto de ambos (lexer y parser)
   con `DiagnosticErrorListener` (ver más abajo) — **sin esto, los
   errores léxicos/sintácticos se van por `stderr` y la IDE nunca los ve**.
3. Llama `parser.program()` para construir el parse tree.
4. Si `reporter.has_errors()` ya es `true` en este punto, retorna
   `nullptr` — el resto del pipeline nunca corre sobre un árbol roto.
5. Si no, `AstBuilder().build(tree)` convierte el parse tree de ANTLR en
   el AST propio.

### `ast_builder.h` / `.cpp` (628 líneas — el segundo archivo más grande del repo)

**Propósito:** el `Visitor` de ANTLR que traduce cada `*Context` del parse
tree a un nodo de `ast::nodes.h`. Ningún método valida ni imprime nada —
solo construye árbol.

**Decisión de diseño clave:** extiende la interfaz **pura**
`CompiscriptVisitor` (no `CompiscriptBaseVisitor`). Esto obliga al
compilador de C++ a que TODOS los ~50 métodos `visitXxx` estén
implementados — si alguien agrega una regla nueva al `.g4` y se olvida de
traducirla, el proyecto no compila. Es una red de seguridad estructural,
no solo estilo.

**Piezas no triviales:**

- **`foldLeftBinary<OperandCtx>()`** — la gramática expresa cada nivel de
  precedencia binaria (`||`, `&&`, `==`/`!=`, `<`/`<=`/`>`/`>=`,
  `+`/`-`, `*`/`/`/`%`) como `operando (op operando)*` (repetición, no
  recursión). Sin este helper, `a - b - c` se plegaría mal. La función
  arma el árbol asociando a la **izquierda**, tomando el operador exacto
  desde `ctx->children[2*i - 1]->getText()`.
- **`visitForStatement()`** — el caso más delicado del archivo. La
  gramática es `'for' '(' (variableDeclaration | assignment | ';')
  expression? ';' expression? ')' block`. El accessor generado por ANTLR
  para `expression(i)` filtra **por tipo entre los hijos presentes**, no
  por "slot lógico" — si falta la condición pero está el update,
  `expression(0)` sería el update, no la condición. Por eso el método
  recorre `ctx->children` a mano contando los `;` literales para saber
  cuál `expression` es cuál.
- **`visitLeftHandSide()`** — pliega manualmente la cadena
  `primaryAtom (suffixOp)*` (`a.b[0].c()`) envolviendo el nodo acumulado
  en cada sufijo: `Call(Access(Index(Access(a, b), 0), c))`. Por esto
  `visitCallExpr`/`visitIndexExpr`/`visitPropertyAccessExpr` existen pero
  **nunca se invocan en el pipeline normal** — son overrides vacíos que
  solo satisfacen la interfaz pura; el trabajo real ocurre acá.
- **`visitLiteralExpr()`** — la gramática define `Literal`,
  `IntegerLiteral` y `StringLiteral` como reglas de lexer separadas, pero
  por el orden de declaración en el `.g4` el lexer **siempre** emite el
  token genérico `Literal` (nunca los otros dos). El método distingue
  entero de string mirando el primer carácter del texto crudo
  (`"` → string).
- **`asBlock()`** — `visitBlock` guarda su resultado como `StatementPtr`
  (misma convención que toda sentencia), así que un `any_cast<BlockPtr>`
  directo falla (`std::any` no hace conversión polimórfica de
  `shared_ptr`). Hay que `any_cast<StatementPtr>` primero y recién
  después `dynamic_pointer_cast<Block>`.

### `error_listener.h` / `.cpp`

**Propósito:** reemplaza el `ConsoleErrorListener` por defecto de ANTLR.
`syntaxError()` traduce cada error de ANTLR a `reporter_.error(SYN001,
msg, line, column+1)` — el `+1` es porque ANTLR reporta columna 0-based y
el resto del compilador usa 1-based en todos lados.

---

## 2. `src/ast/` — el árbol y su representación visual

### `nodes.h` (356 líneas)

**Propósito:** la jerarquía completa del AST. Cada clase corresponde a
una regla real del `.g4` — verificado línea por línea, no supuesto.

**Diseño de la base `AstNode`:**
```cpp
class AstNode {
    int line = 0, column = 0;
    std::shared_ptr<semantic::Type> resolved_type;  // shared_ptr, no puntero crudo
    semantic::Symbol* symbol = nullptr;              // no propietario
    semantic::Scope* scope = nullptr;                 // no propietario
};
```
`resolved_type` es deliberadamente `shared_ptr<Type>` y no un puntero
crudo: muchos `Type` se sintetizan al vuelo durante el chequeo (p. ej. el
tipo resultado de un `BinaryExpression`) y no tienen otro dueño estable
— un puntero crudo ahí quedaría colgante en cuanto termine el `shared_ptr`
local que lo creó. `symbol`/`scope` sí son crudos porque `Symbol`/`Scope`
viven con vida propia en el árbol de scopes.

`nodes.h` **forward-declara** `semantic::Symbol/Scope/Type` en vez de
incluir sus headers reales — si incluyera `semantic/symbol.h` de verdad
se generaría un include circular (`symbol.h` ya incluye `ast/nodes.h`
para `TypeAnnotation`).

**~30 clases de nodo**, cada una con un comentario citando la regla exacta
del `.g4` que representa. Detalles que vale la pena recordar:
- `ForStatement::init` puede ser `VariableDeclaration`, `AssignmentStatement`
  o `nullptr` (si el slot fue solo el `;` literal).
- `AssignmentStatement` (statement) es un nodo **distinto** de
  `AssignmentExpression` (expresión) — la gramática los separa porque
  `assignment` a nivel de sentencia no permite encadenar (`x = y = 3;`
  como sentencia no es la misma regla que `x = y = 3` como subexpresión).
- `FunctionDeclaration` no tiene un flag "es constructor": un método
  llamado literalmente `"constructor"` **es** el constructor por
  convención de nombre, en cualquier parte del pipeline que lo necesite.
- `LiteralKind` no tiene `Float` — no existe en la gramática.

### `printer.h` / `.cpp` (264 líneas)

**Propósito:** produce tanto el texto indentado del AST como el `.dot`
para Graphviz, a partir de **una sola función**, `describe()`.

**El diseño central:** `describe(const AstNode*) → NodeView{label,
children}` concentra TODO el conocimiento de "cómo se ve cada tipo de
nodo" en un único lugar — un `dynamic_cast` en cascada por cada clase de
`nodes.h`. Tanto `printTextRec()` (texto indentado, con `(linea L, col
C)` en cada línea) como `toDotRec()` (grafo DOT con nodos escapados y
aristas etiquetadas por rol: `"cond"`, `"then"`, `"body"`, etc.) llaman
a esta misma función — si mañana se agrega un nodo nuevo, se actualiza
`describe()` una vez y ambos formatos de salida quedan correctos.

`printTree()` es exactamente lo que imprime el CLI bajo `AST:`; `toDot()`
es lo que se escribe a `output/ast.dot`.

---

## 3. `src/diagnostics/` — el contrato de códigos

### `diagnostic.h`

`struct Diagnostic { Severity severity; std::string code; std::string
message; int line, column, length=1; }`. `length` existe para que un IDE
más completo pueda subrayar un rango, aunque hoy nada lo usa más allá de
`1`.

### `codes.h`

Solo constantes `constexpr const char*` — un código por línea, a
propósito, para que los tests comparen **códigos** (`"SEM011"`) y no
mensajes en texto libre (frágiles ante cualquier cambio de redacción).
`SYN001` + `SEM001`–`SEM020` (ver tabla completa en el doc 06).

### `reporter.h` / `.cpp`

**Propósito:** `DiagnosticReporter` acumula diagnósticos durante una
compilación. **Una instancia nueva por cada `Compiler::compile()`** —
comentario explícito en el código: sin esto, un contador que no se
reinicia produciría bugs que solo aparecen en la IDE (donde el mismo
proceso compila muchas veces seguidas) y nunca en los tests de C++ (donde
cada caso arranca un proceso nuevo). Esto es exactamente lo que hace
segura la reutilización del mismo binario desde la IDE sin reiniciar el
proceso.

- `error()`/`warning()` son azúcar sobre `report(Severity, ...)`.
- `has_errors()` usa `std::any_of` — es lo que decide si el pipeline
  sigue acumulando diagnósticos o si un error léxico/sintáctico corta
  antes del AST.
- `sorted()` devuelve una copia ordenada por `(line, column)` con
  `stable_sort` — los diagnósticos se generan en orden de **recorrido**
  del AST, no necesariamente en orden de aparición en el código fuente
  (un error semántico puede reportarse "fuera de orden" respecto a uno
  sintáctico), así que el CLI ordena antes de imprimir.

---

## 4. `src/compiler/` — la fachada

### `compiler.h` / `.cpp`

`Compiler::compile(source)` es un método **estático** que arma:
```cpp
DiagnosticReporter reporter;                    // nueva cada vez
CompilationResult result;
result.ast = frontend::parse(source, reporter);

if (result.ast != nullptr) {
    DeclarationCollector(...).run(*result.ast);
    InheritanceResolver(...).run(*result.ast);
    NameResolver(...).run(*result.ast);
    TypeChecker(...).run(*result.ast);
    ControlFlowChecker(...).run(*result.ast);
    ClosureAnalyzer().run(*result.ast);
}
result.diagnostics = reporter.sorted();
```
El orden es fijo y **no es arbitrario** — cada pase asume que el
anterior ya corrió (ver sección 5 para el porqué exacto de cada
dependencia).

Si `result.ast == nullptr` (error sintáctico), ningún pase semántico
corre — no tiene sentido resolver nombres o tipos sobre un árbol que no
se pudo construir.

### `result.h`

`CompilationResult { ast, diagnostics, symbol_table }`. `symbol_table`
es **siempre válido** (nunca null) — si hubo error sintáctico, queda con
el scope global vacío en vez de obligar a quien consume el resultado a
chequear null primero. `success()` es `true` si ningún diagnóstico tiene
`Severity::Error` (warnings no cuentan).

---

## 5. `src/semantic/` — los 6 pases, en el orden en que corren

Antes de entrar a cada archivo: **por qué este orden y no otro**.

```
DeclarationCollector   necesita: nada (primero)
        ↓ construye scopes + declara todos los simbolos
InheritanceResolver     necesita: TODAS las clases ya declaradas
        ↓ resuelve base_class_name -> base_class, detecta ciclos
NameResolver             necesita: simbolos declarados
        ↓ resuelve identificadores; NO mira tipos ni herencia todavia
TypeChecker              necesita: nombres resueltos + herencia resuelta
        ↓ prepara firmas (fase previa) y luego valida tipos/miembros
ControlFlowChecker       necesita: TypeChecker (para SEM020, ver abajo)
        ↓ break/continue/return, codigo muerto
ClosureAnalyzer           necesita: nombres y scopes resueltos
        ↓ solo metadata, no reporta diagnosticos
```

### `symbol.h`

Los tres tipos de símbolo. Campos que no son obvios por el nombre:

- `Symbol::is_rejected` — `true` si este símbolo perdió una
  redeclaración (`SEM002`). Se conserva vivo (no se descarta) para que
  el nodo del AST que lo generó siga teniendo un `symbol` válido —
  recuperación de errores real, ver `scope.cpp` abajo.
- `Symbol::declaring_scope` — puntero directo al `Scope` donde se
  declaró. `ClosureAnalyzer::capture()` lo usa para decidir "¿esta
  variable es local a la función actual o viene de afuera?" sin tener
  que resolver el nombre otra vez.
- `Symbol::declaration` — el `ast::Statement*` que lo declaró (solo para
  variables/constantes). Es la pieza clave de la **inferencia de tipo
  perezosa**: si `TypeChecker::symbolType()` necesita el tipo de un
  símbolo que todavía no tiene `resolved_type`, dispara
  `checkStatement(symbol->declaration)` sobre la marcha (ver más abajo).
- `FunctionSymbol::captured` / `captured_this` — los llena
  `ClosureAnalyzer`, no validan nada; son metadata para cuando el curso
  llegue a generación de código real (y ya los consume
  `descriptor_builder.cpp` en `src/runtime/`).
- `ClassSymbol::base_class` — el puntero YA resuelto por
  `InheritanceResolver`, distinto de `base_class_name` (el string tal
  como se escribió en `class Perro : Animal`).

### `scope.h` / `.cpp`

**Diseño:** "pila de tablas" — cada `Scope` tiene sus propios símbolos
(`unordered_map<string, SymbolPtr>`) y un puntero `parent_`. El árbol de
scopes es dueño de sus hijos (`vector<unique_ptr<Scope>>`); todo lo
demás navega con punteros crudos no propietarios.

```cpp
SymbolPtr Scope::declare(SymbolPtr symbol) {
    symbol->declaring_scope = this;
    auto existing = symbols_.find(symbol->name);
    if (existing != symbols_.end()) {
        symbol->is_rejected = true;
        rejected_symbols_.push_back(symbol);   // <- recuperacion de errores
        return existing->second;               // <- para que el llamador arme SEM002
    }
    symbols_[symbol->name] = symbol;
    return nullptr;                            // exito
}
```
Esta es **la** pieza de recuperación de errores del proyecto: antes, una
declaración duplicada simplemente se perdía (el segundo `Symbol` nunca
se guardaba en ningún lado y su nodo del AST quedaba con `symbol =
nullptr`, lo cual podía disparar diagnósticos secundarios en cascada más
abajo en el pipeline). Ahora se guarda en `rejected_symbols_` — no
participa de `resolve()`/`resolveLocal()` (no contamina búsquedas), pero
sigue vivo, con `is_rejected = true` y `declaring_scope` fijado, así que
pases posteriores (`TypeChecker::prepareScope`, por ejemplo) pueden
seguir procesándolo sin `nullptr` checks especiales.

`resolve()` sube por `parent_` hasta el scope global (ámbito léxico
normal); `resolveLocal()` solo mira el scope actual (se usa para decidir
si algo es redeclaración).

### `symbol_table.h`

Trivial a propósito: dueño único del scope global
(`unique_ptr<Scope>`), del que cuelga todo el árbol.

### `type.h` / `.cpp`

**`TypeKind`:** `Integer, String, Boolean, Null, Void, Array, Function,
Class, EmptyElement, Error` — sin `Float` (no existe en la gramática).

**`Type::equals()` — la regla de compatibilidad central de todo el
sistema de tipos:**
1. Si `kind == Error` en cualquiera de los dos lados → `true` siempre.
   Esto es lo que evita las cascadas de errores: una vez que algo ya
   falló, cualquier operación posterior sobre ese valor se considera
   "compatible" para no generar un segundo (tercer, cuarto...)
   diagnóstico sobre el mismo problema original.
2. `Null` es compatible con tipos de **referencia** (`Class`, `Array`,
   `Function`) en cualquiera de los dos lados, pero NO con primitivos —
   `let p: Perro = null;` es válido, `let x: integer = null;` no.
3. `EmptyElement` (el tipo de `[]` sin contexto) es compatible con
   cualquier cosa, sin usar `Error` como comodín — así `[]` no dispara
   un diagnóstico solo por no tener aún un tipo de elemento concreto.
4. Si los `kind` son distintos después de estas excepciones → `false`.
5. Comparación estructural real para `Array` (recursiva sobre
   `element_type`), `Class` (identidad de puntero `ClassSymbol*` — dos
   clases son el mismo tipo solo si son literalmente la misma
   declaración) y `Function` (misma cantidad de parámetros, cada uno
   compatible, y retorno compatible).

**`commonType(left, right)`** — unifica dos tipos para arreglos y
ternarios: si alguno es `EmptyElement`/`Null`, gana el otro; si ambos son
`Array`, unifica recursivamente sus `element_type`; si no, se queda con
`left`. Se usa en `ArrayLiteral` (unificar el tipo de todos los
elementos) y en `TernaryExpression` (unificar `then`/`else`).

**`resolveTypeAnnotation()`** — convierte lo que el programador escribió
(`ast::TypeAnnotation`, un string básicamente) en un `Type` resuelto:
`"integer"/"boolean"/"string"` son directos; cualquier otro nombre se
busca como clase en `scope` (`SEM013` si no existe o no es clase);
`ArrayTypeAnnotation` se resuelve recursivamente elemento por elemento.

### `declaration_collector.h` / `.cpp` — Pase 1

**Propósito:** construye TODO el árbol de scopes y declara cada símbolo,
en un único recorrido. No resuelve referencias, no valida tipos.

**Reglas de scope que vale la pena memorizar:**
- El cuerpo de una función **comparte** `function_scope` directamente
  con sus parámetros — no hay un `Block` scope adicional anidado para el
  cuerpo inmediato (mismo criterio que el diagrama típico de apuntes de
  clase: un `FunctionScope` contiene parámetros y locales al mismo
  nivel).
- `for (let i = 0; ...)`: la variable de `init` vive en un `Scope::Block`
  propio del `for` (no en el scope contenedor), para que no se filtre
  fuera del loop — `n->scope` en `ForStatement` es ese scope propio.
- `switch`: la gramática no pone `{ }` alrededor de cada `case`, así que
  no hay límite sintáctico para darle scope propio — **todos los
  `case` y el `default` comparten el scope del `switch`**.
- `catch (id)`: el parámetro recibe sintéticamente
  `NamedTypeAnnotation("string")` como `declared_type` — la gramática no
  permite anotar su tipo, así que se fija a `string` acá mismo, en la
  construcción del símbolo.
- `foreach (x in expr)`: `x` se declara con `declared_type = nullptr`
  (nunca se anota en la gramática); su tipo real se infiere después, en
  `TypeChecker`, a partir del tipo de elemento del arreglo iterado.

`declareOrReport()` es el único punto donde se llama `scope->declare()`
y se reporta `SEM002` si vuelve un conflicto — usa la línea del símbolo
**existente** (`conflict->declared_line`) en el mensaje, para que el
error diga "ya declarado en la línea X" con la línea original, no la
nueva.

### `inheritance_resolver.h` / `.cpp`

**Dos pasadas, a propósito:**
1. **Resolver primero, para TODAS las clases**, cada
   `base_class_name` (string) a un `ClassSymbol*` real
   (`SEM013` si el nombre no resuelve a una clase). Esto se hace para
   todas antes de buscar ciclos porque una clase puede heredar de otra
   declarada **más abajo** en el archivo — como `DeclarationCollector`
   ya declaró todas antes de que este pase corra, el orden textual no
   importa.
2. **Después**, con todos los `base_class` ya enlazados,
   `detectCycle()` camina la cadena `base_class` de cada clase con un
   `std::set<ClassSymbol*>` — si un `ClassSymbol` reaparece, es un
   ciclo (`SEM014`).

`collectClasses()` busca `ClassDeclaration` en **cualquier profundidad**
del árbol (dentro de bloques, if, while, funciones...) — una clase
declarada dentro del cuerpo de una función es sintácticamente legal,
aunque poco común.

### `name_resolver.h` / `.cpp` — Pase 2

**Propósito:** recorre statements Y expresiones (a diferencia de
`DeclarationCollector`, que solo mira statements) resolviendo cada
identificador contra `scope->resolve()`. `SEM001` si no existe.

**Explícitamente NO resuelve todavía** (el comentario en el header lo
deja claro): nombres de miembro (`obj.campo`), la palabra `this`, ni
número/tipo de argumentos en llamadas — todo eso requiere saber el
**tipo** del objeto, y eso es trabajo de `TypeChecker`. Este pase se
limita a nombres del entorno léxico puro.

Cada `resolveExpression()` deja `expr->scope = scope` de paso — es lo
que después usa `ClosureAnalyzer` (y en su momento usaba, de forma más
directa, la versión vieja del propio `ClosureAnalyzer`) para saber en
qué scope se usó cada identificador.

### `type_checker.h` / `.cpp` — Pase 3 (624 líneas, el archivo más grande)

El pase que más reglas del PDF cubre por sí solo. Se divide en dos
etapas claramente separadas en el código:

#### Etapa A — `prepareScope()` / `prepareSymbol()`: preparación de firmas

Antes de revisar UN SOLO cuerpo de función, `run()` llama
`prepareScope(program.scope)`, que camina **todo** el árbol de scopes
(incluyendo `rejectedSymbols()`, para que hasta los símbolos rechazados
por `SEM002` terminen con un `resolved_type` coherente) y para cada
símbolo:
- Clase → `resolved_type = makeClassType(cls)`.
- Función → resuelve el tipo de cada parámetro y el tipo de retorno
  (`Void` si no se anotó), arma `makeFunctionType(params, return)`.
- Variable/constante con anotación → resuelve esa anotación (los
  parámetros se resuelven contra el scope **padre** de la función, no
  contra sus propios locales — para que un parámetro no pueda "ocultar"
  el nombre de una clase que necesita para su propio tipo).
- Parámetro **sin** anotación → `SEM011` inmediato, ahí mismo.

**Esto es lo que arregla la limitación de "referencia hacia adelante"**
documentada en la sesión anterior: como todas las firmas ya están
resueltas antes de revisar cualquier cuerpo, una función puede llamar a
otra declarada más abajo en el archivo sin que la validación de
argumentos falle silenciosamente.

#### Etapa B — `checkStatement()` / `checkExpression()`: el chequeo real

Recorrida estándar bottom-up (atributos sintetizados: el tipo de un nodo
se calcula desde sus hijos). Piezas que vale la pena entender a fondo:

- **`symbolType()` + inferencia perezosa con memoización.** Si un
  símbolo todavía no tiene `resolved_type` pero sí tiene
  `symbol->declaration` (el `Statement*` que lo declaró), dispara
  `checkStatement(symbol->declaration)` ahí mismo. Como `checkStatement`
  guarda cada statement ya visitado en `checked_`
  (`std::unordered_set<Statement*>`) y no vuelve a procesarlo, esto
  resuelve **inferencia de tipo sin anotación explícita**
  (`let x = otraFuncion();`) sin importar el orden textual, sin
  recomputar dos veces, y sin loop infinito si dos variables se
  infieren mutuamente (en ese caso, la segunda ve que la primera
  todavía no tiene tipo resuelto y cae al camino de `SEM019`).
- **`checkLvalue()` / `checkWritable()`** — separan dos preguntas
  distintas: "¿esto es sintácticamente un destino de asignación válido?"
  (identificador, `.miembro`, o `arr[i]`) de "¿el símbolo detrás es
  mutable?" (no `const`, no función, no clase). Un `arr[i] = x` es
  válido aunque `arr` sea `const` — `const` impide reasignar la
  **referencia**, no mutar los elementos del arreglo que ya referencia.
- **Operador `+` con regla especial** — es el único operador binario
  que NO sigue el patrón genérico "ambos operandos del mismo tipo
  esperado": permite `integer+integer` o `string+string`, pero rechaza
  explícitamente mezclar (`SEM004`, mensaje literal "no se permite
  mezclar"). El resto de aritmética (`- * / %`) exige `integer` en
  ambos lados sin excepción.
- **`ArrayLiteral`** — unifica el tipo de todos sus elementos con
  `commonType()`, reportando `SEM004` en el primer elemento incompatible
  pero **sin cortar el recorrido** (sigue revisando el resto de
  elementos para no perder otros diagnósticos reales). Un `[]` vacío
  produce `makeArrayType(makeEmptyElementType())` en vez de forzar
  `Error`.
- **`NewExpression`** — busca `"constructor"` como método vía
  `lookupMember()` (que ya sube por la cadena de herencia). Si no hay
  constructor definido y se pasaron argumentos igual, es `SEM008`
  ("la clase no tiene constructor, pero se le pasaron argumentos" — el
  PDF dice "si existe" el constructor, así que sin uno definido no
  debería aceptar argumentos). Si `constructor` existe pero no es un
  método (poco probable dado que `constructor` siempre se declara como
  `FunctionDeclaration`, pero el chequeo está por seguridad), `SEM017`.
- **`CallExpression`** — si el tipo del callee no es `Function`, `SEM016`
  ("el valor no es invocable"), salvo que ya sea `Error` (evita
  cascada).
- **`ArrayAccessExpression`** — si el arreglo tiene `element_type ==
  EmptyElement` (un `[]` sin anotar, todavía sin contexto que le dé
  tipo), indexarlo es `SEM019` ("anote el tipo del arreglo vacío antes
  de acceder a sus elementos") en vez de dejarlo pasar silenciosamente.
- **`resolveMemberAccess()` + `lookupMember()`** — el helper compartido
  entre `MemberAccessExpression`, `PropertyAssignment` y
  `PropertyAssignExpr`. `lookupMember()` sube por `base_class` con un
  `unordered_set<ClassSymbol*> visited` (protección extra contra ciclos,
  aunque `InheritanceResolver` ya debería haberlos cortado antes).

### `control_flow_checker.h` / `.cpp` — Pase 4

**Corre después de `TypeChecker`** por una razón concreta: `SEM020`
(función que puede terminar sin retornar) necesita
`n->return_type->resolved_type->kind`, que solo existe una vez que
`TypeChecker::prepareSymbol()` ya lo resolvió.

**El corazón del archivo: `flowOf()`, un resumen de flujo por bitmask**,
sin construir un CFG (grafo de flujo de control) real ni ejecutar nada:

```cpp
enum Flow : unsigned { FallsThrough=1, Returns=2, Breaks=4, Continues=8 };
```

Cada statement (y cada lista de statements, y cada bucle) se resume en
un `unsigned` que dice **qué combinación de esos 4 caminos es posible**
al terminarlo. Ejemplos de cómo se combinan:
- Una lista de statements: se recorre en orden; si el statement actual
  ya no puede "caer al siguiente" (`FallsThrough` apagado), el resto de
  la lista es código muerto y el recorrido de flujo se corta ahí (los
  flags de `Returns/Breaks/Continues` del último statement alcanzado son
  el resultado final).
- `IfStatement`: `flowOf(then) | flowOf(else ?? FallsThrough)` — el
  resultado es la **unión** de ambas ramas (si alguna rama puede caer al
  siguiente statement, el `if` completo puede).
- **`loopFlow()`** — el caso más sutil: sabe distinguir `while(true)` /
  `for(;;)` (condición omitida en un `for` cuenta como "siempre
  verdadera", parámetro `omittedMeansTrue`) de una condición literal
  `false`, y sabe que un `do-while` (`runsOnce = true`) ejecuta el
  cuerpo al menos una vez aunque la condición sea falsa. Un `break`
  dentro del cuerpo hace que el loop completo pueda caer al siguiente
  statement (`FallsThrough`); un `Returns` del cuerpo se propaga tal
  cual hacia afuera; `break`/`continue` quedan **consumidos** por ese
  mismo bucle (no se propagan más arriba — eso ya lo valida por
  separado `checkStatement` con las banderas `insideLoop`).
- `SwitchStatement`: recorre los `case` de atrás hacia adelante
  acumulando "cola" (`tail`) para modelar el fallthrough real de
  `switch` (si un `case` no corta con `break`/`return`, cae al
  siguiente) — sin default, el switch completo siempre puede caer al
  statement siguiente.

`checkDeadCode()` usa ese mismo `flowOf()` para reportar `SEM012` **una
sola vez por lista de statements**, en el primer statement inalcanzable
(no un diagnóstico por cada línea muerta subsiguiente).

**`checkStatement()`** es el recorrido "clásico" con dos banderas
(`insideLoop`, `insideFunction`) que valida `SEM006`/`SEM007`
directamente, y dispara `SEM020` en cada `FunctionDeclaration` cuyo tipo
de retorno no sea `Void`/`Error` y cuyo `flowOf(body)` todavía tenga
`FallsThrough` prendido. **`switch` no cuenta como bucle**: al entrar a
sus `case`, `checkStatement` propaga `insideLoop` tal cual venía (si ya
había un bucle real más afuera, `break`/`continue` siguen siendo
válidos ahí dentro; si no, siguen sin serlo) — decisión de lenguaje
documentada en el header del archivo.

### `closure_analyzer.h` / `.cpp` — Pase 5 (no reporta diagnósticos)

**Propósito:** anotar `FunctionSymbol::captured` y `captured_this` — pura
metadata para una futura generación de código, sin validar nada.

**`capture(symbol, funcScope)`** es el único punto de decisión:
1. Si el símbolo no tiene `declaring_scope`, no hace nada.
2. Si se declaró en un `Scope::Class` (es un atributo/método de la
   clase contenedora, accedido implícitamente), delega en
   `captureThis()` en vez de tratarlo como variable capturada.
3. Excluye explícitamente funciones, clases y símbolos globales — **no
   cuentan como captura** (una corrección real sobre la versión
   anterior, que sí las contaba por error).
4. Si el símbolo se declaró dentro de la propia `funcScope` (recorriendo
   hacia arriba desde `declaring_scope`), es un local — no es captura.
5. Si no, sube desde `funcScope` hacia el scope de declaración
   registrando el símbolo en `captured` de **cada** `FunctionSymbol`
   intermedio que encuentre en el camino (`scope->kind() ==
   ScopeKind::Function`) — así una captura se propaga correctamente a
   través de funciones anidadas varios niveles.

**`captureThis(cls, funcScope)`** — similar pero para el receptor
léxico: sube desde `funcScope` marcando `captured_this = cls` en cada
`FunctionSymbol` anidado que no sea directamente el método (el método en
sí ya recibe `this` de forma natural; son las funciones **interiores**
al método las que necesitan capturarlo).

`findFunctions()` recorre el árbol completo buscando cada
`FunctionDeclaration` (a cualquier profundidad) y dispara
`analyzeFunctionBody()` por separado para cada una — una función anidada
detectada durante el recorrido de su contenedora se analiza aparte
(contexto propio), no se vuelve a buscar desde el nivel superior.

### `printer.h` / `.cpp`

**Propósito:** produce el texto que el CLI imprime bajo `Tabla de
simbolos:` — un recorrido recursivo de todo el árbol de `Scope`,
imprimiendo cada símbolo con su "firma" (`signatureText()`):
tipo declarado para variables/constantes, `(params) -> retorno` para
funciones (con `[captura: ...]` y `[captura this: Clase]` si aplica), y
`extiende X` / `sin base` para clases.

**Detalle no obvio:** `printRec()` solo itera `scope->symbols()` — los
`rejectedSymbols()` (declaraciones duplicadas conservadas para
recuperación) existen y se procesan en `TypeChecker::prepareScope()`,
pero **no aparecen en el árbol impreso**. Son metadata interna de
recuperación, no algo que el usuario final deba ver en la tabla de
símbolos.

---

## 6. `src/runtime/` — scaffold de GC, fuera de la rúbrica de este proyecto

Nada acá se ejecuta durante `Compiler::compile()` — es infraestructura
lista para cuando el curso llegue a generación de código real.

### `type_descriptor.h`

Puros structs de datos: `SlotKind` (`Primitive` | `Pointer` — si el
`mark` del GC debe seguir ese slot como puntero al heap o tratarlo como
dato plano), `FieldSlot` (nombre + kind + índice de palabra dentro del
payload), `ClassDescriptor` (lista ordenada de `FieldSlot`, incluyendo
heredados), `ArrayDescriptor` (un `SlotKind` único para todos los
elementos, porque los arreglos son homogéneos).

### `heap.h` / `.cpp`

**Propósito:** el único lugar autorizado a reservar memoria dinámica del
runtime. `HeapObject` guarta un `ObjectHeader` (kind, marcado, descriptor
de clase, cantidad de palabras) y un `payload` (`vector<uintptr_t>`)
donde cada palabra es o un valor primitivo o un puntero a otro
`HeapObject` reinterpretado (`setPointer`/`getPointer` hacen el
`reinterpret_cast`).

`sweepUnmarked()` es el "sweep" del mark-and-sweep: mueve a un vector
`survivors` solo los objetos con `header.marked == true`, resetea ese
flag para el próximo ciclo, y descarta (destruye, vía `unique_ptr`) el
resto.

### `gc.h` / `.cpp`

**Mark-and-sweep clásico**, con dos decisiones de diseño explícitas en
los comentarios:
- **Maneja ciclos** (a diferencia de conteo de referencias) — un objeto
  que se referencia a sí mismo pero no es alcanzable desde ninguna raíz
  se recolecta igual.
- **`markObject()` usa una pila explícita (`std::stack`), no recursión de
  C++** — para no depender del stack de C++ del propio colector (un
  grafo de objetos muy profundo no podría desbordar el stack real del
  programa).

`collect()` = `markFromRoots()` (marca cada raíz registrada vía
`addRoot()`, y desde ahí sigue cada `FieldSlot`/elemento de arreglo
marcado como `Pointer` en el descriptor) + `heap_.sweepUnmarked()`.
`allocClass()`/`allocArray()` disparan `collect()` automáticamente si el
heap ya alcanzó `threshold_objects_` antes de reservar — el "punto
seguro" típico de un GC simple de curso.

### `descriptor_builder.h` / `.cpp`

**Propósito:** construye `ClassDescriptor`/`ArrayDescriptor` a partir de
la tabla de símbolos **ya resuelta** por los pases semánticos — no
recalcula nada, solo lee `ClassSymbol::class_scope` y
`Symbol::resolved_type`.

`collectClassFields()` recorre la cadena de herencia **base primero**
(recursión antes de agregar los campos propios), así el layout de
memoria de una clase derivada siempre tiene los campos heredados en las
mismas posiciones que la base — necesario para que un puntero a la
subclase se pueda tratar como puntero a la base sin reordenar memoria.
Descarta símbolos `is_rejected` (declaraciones duplicadas) y symbols que
no son `Variable`/`Constant` (los métodos no ocupan slot de instancia).
Ordena los campos por nombre antes de asignar `word_index` — determinismo
explícito, porque `Scope::symbols_` es un `unordered_map` sin orden
garantizado.

`walkScope()` recorre todo el árbol de scopes una vez, construyendo un
`ClassDescriptor` por cada clase encontrada y una `ClosureCaptureInfo`
por cada función con `captured`/`captured_this` no vacíos — esta es la
pieza que conecta el trabajo de `ClosureAnalyzer` (Pase 5) con el mundo
de generación de código futuro.

---

## 7. Mapa de dependencias entre archivos (quién incluye a quién, en la práctica)

```
ast/nodes.h  <-- (todo lo demás; es la base común)
diagnostics/{diagnostic,codes,reporter}.h  <-- todos los pases y frontend/
semantic/type.h  <-- symbol.h  <-- scope.h  <-- symbol_table.h
                                        ^
semantic/{declaration_collector, inheritance_resolver, name_resolver,
          type_checker, control_flow_checker, closure_analyzer, printer}
    -- cada uno incluye scope.h/symbol.h/type.h segun necesite, y
       diagnostics/codes.h para los codigos que reporta.

frontend/parser_driver.h  <-- compiler/compiler.cpp  (unico punto donde
                               el resto del compilador "toca" ANTLR,
                               indirectamente, a traves de esta fachada)

compiler/compiler.cpp  --> orquesta los 6 pases semanticos en orden fijo

runtime/*  --> solo depende de semantic/{scope,symbol,type}.h y de
               symbol_table.h (lectura), nunca al reves: nada en
               semantic/ ni compiler/ sabe que runtime/ existe.
```
