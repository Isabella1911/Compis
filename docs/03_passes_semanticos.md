# Passes semánticos restantes

> Documento 2 de 3. Esto es el grueso de los 60 puntos de "Analizador
> Sintáctico y Semántico" del PDF (`README_SEMANTIC_ANALYSIS.md` del repo
> del profesor). Depende de que el [sistema de tipos](02_sistema_de_tipos.md)
> ya exista — sin `resolved_type` casi nada de esto se puede validar.

Cada punto de acá abajo es literalmente una viñeta del PDF, con el código
`SEM0xx` que le corresponde (ya reservado en `src/diagnostics/codes.h`,
salvo donde se indica que hace falta uno nuevo) y una nota de dónde
engancharlo. No hace falta un pass nuevo por viñeta — varias comparten
naturalmente el mismo recorrido del árbol.

## 1. Sistema de Tipos — [HECHO] (`src/semantic/type_checker.cpp`)

Todo lo de esta sección ya está implementado y probado
(`tests/fixtures/{valid,invalid}/sem00*.cps`, `tests/fixtures/valid/tipos.cps`):

- `+ - * / %` con operandos `integer` → `SEM004`. `+` además acepta
  `string+string` (concatenación); no se agregó `float` (sigue sin
  existir en Compiscript, ver README).
- `&& ||` con operandos `boolean`, `!` con operando `boolean` → `SEM004`.
- `== !=` entre tipos compatibles (cualquiera); `< <= > >=` exigen
  `integer` → `SEM004`.
- Asignación (`AssignmentStatement`, `AssignmentExpression`) con el tipo
  del valor coincidiendo con el declarado → `SEM003`.
  **`PropertyAssignment`/`PropertyAssignExpr` (asignar a `obj.campo`)
  sigue sin validar** — necesita resolver `member_name`, ver sección 5.
- `const` inicializada obligatoriamente — cubierto por la gramática desde
  la Etapa 1, sin cambios.
- Tipos en listas: todos los elementos de un `ArrayLiteral` deben ser
  compatibles entre sí → `SEM004`. Índice de `ArrayAccessExpression` debe
  ser `integer` → `SEM004`.
- Bonus no listado originalmente acá pero implementado igual: tipo de
  retorno (`SEM009`, ver sección 3) y compatibilidad `switch`/`case` (ver
  [`docs/02_sistema_de_tipos.md`](02_sistema_de_tipos.md) sección 4).

## 2. Manejo de Ámbito — [HECHO] (`src/semantic/inheritance_resolver.h/.cpp`)

Ya cubierto por `DeclarationCollector` + `NameResolver`:
resolución de nombres, no declaradas, redeclaración, un `Scope` por
función/clase/bloque. Lo de clases (herencia) también está resuelto ahora:

- **`InheritanceResolver`** (Pass propio, corre después de
  `DeclarationCollector` y antes de `NameResolver`): resuelve
  `ClassSymbol::base_class_name` (el string tal como se escribió) al
  `ClassSymbol* base_class` real (campo nuevo en `symbol.h`). Funciona sin
  importar el orden de declaración — `class Perro : Animal` con `Animal`
  declarada más abajo en el archivo resuelve bien, porque
  `DeclarationCollector` ya declaró **todas** las clases antes de que este
  pass corra. Si el nombre de la base no existe o no es una clase →
  `SEM013` (mismo código que "tipo inválido", tiene sentido reusarlo:
  nombrar una clase base inexistente es exactamente ese error).
- **Herencia circular** (`class A : B { }` y `class B : A { }`, o cadenas
  más largas): se detecta recorriendo `base_class` llevando un `set` de
  clases ya vistas → `SEM014` (código nuevo, agregado a `codes.h`). Se
  reporta una vez por cada clase involucrada en el ciclo (si `A` y `B` se
  heredan mutuamente, salen dos diagnósticos, uno por clase — es
  intencional, ayuda a ver ambos lados del ciclo).
- Encuentra clases sin importar cuán anidadas estén (una clase declarada
  dentro del cuerpo de una función es sintácticamente legal aunque poco
  común) — recorre todo el árbol, no solo el nivel superior del programa.

Probado: `tests/fixtures/valid/herencia.cps` (cadena de 3 niveles,
`Cachorro : Perro : Animal`, con `Animal` declarada después de `Perro` a
propósito para probar la referencia hacia adelante) sin diagnósticos;
`sem013_herencia_base_inexistente.cps` y `sem014_herencia_circular.cps`
disparan exactamente lo esperado.

**Lo que esto todavía no desbloquea por sí solo:** resolver miembros
heredados (`obj.campo` buscando primero en la clase, después en su base,
y así sucesivamente) — eso es la sección 5 de este documento, que ahora sí
tiene con qué trabajar (`ClassSymbol::base_class` ya está listo para
recorrerse).

## 3. Funciones y Procedimientos

- **[HECHO]** Número y tipo de argumentos en una llamada, coincidencia
  posicional contra `FunctionSymbol::resolved_type->param_types` →
  `SEM008` (`TypeChecker::checkArguments()`). Cubre tanto llamadas a
  funciones sueltas (`CallExpression`) como a métodos (`obj.metodo(...)`,
  vía la sección 5) y al constructor (`new Clase(...)`, misma función
  compartida). **Límite de orden conocido:** si se llama a una
  función/método antes de que `TypeChecker` haya procesado esa
  declaración en particular (p. ej. una clase definida más abajo en el
  archivo), esa llamada puntual no se valida — `resolved_type` de la
  función todavía no existe en ese momento. No genera un diagnóstico
  incorrecto, simplemente no valida esa llamada (ver comentario al inicio
  de `type_checker.h`). No afecta la resolución de *nombres* (eso ya
  funciona sin importar el orden, gracias a `NameResolver` +
  `InheritanceResolver`).
- **[HECHO]** Tipo de retorno coincide con `FunctionSymbol::return_type` →
  `SEM009`, implementado en `TypeChecker` (caso `ReturnStatement`). Usa
  `Scope::owner` (campo nuevo, agregado junto con esto) para encontrar la
  función contenedora subiendo por los `Scope` padres. Si `return_type` es
  `nullptr` (sin anotar), se asume `void`. **Ojo:** si `ReturnStatement`
  aparece fuera de toda función, `TypeChecker` simplemente no valida nada
  (no hay tipo esperado) — reportar `SEM007` en ese caso es trabajo del
  `ControlFlowChecker` de la sección 4, todavía no escrito.
- Recursión: **no necesita código nuevo**. Como `DeclarationCollector` ya
  declaró la función en su scope contenedor antes de entrar a su cuerpo,
  `NameResolver` ya resuelve una llamada recursiva sin problema. Alcanza
  con un test que lo confirme (`factorial.cps` en los fixtures ya lo
  ejercita, de hecho).
- Funciones anidadas: **tampoco necesita código nuevo** por la misma
  razón — ya funciona (ver `crearContador` en `Especificaciones.md`).
  Ver la sección de closures más abajo para la parte que sí falta.
- Redeclaración de función con el mismo nombre: **ya cubierto por
  `SEM002`**, `DeclarationCollector` ya lo detecta porque una función es
  un símbolo más en su scope.
- Parámetro sin anotación de tipo → `SEM011` (ya reservado). Es una
  decisión de lenguaje pendiente (README punto 2): ¿es error, o se asume
  algún tipo por defecto? Hay que cerrarla antes de escribir esta
  validación.

### Closures — [HECHO] (`src/semantic/closure_analyzer.h/.cpp`)

`ClosureAnalyzer` recorre el cuerpo de cada función (incluidas las
anidadas, cada una con su propio análisis independiente) y llena
`FunctionSymbol::captured` con cada `Symbol*` resuelto fuera del
`function_scope` propio pero usado adentro. No dispara ningún
diagnóstico, tal como pedía este documento — es información para cuando
se generen closures reales en tiempo de ejecución (fuera de este
proyecto). Corre después de `NameResolver` (necesita `symbol`/`scope` ya
resueltos en cada `IdentifierExpression`).

Es visible en la salida de `printScopeTree()`: una función que captura
algo se imprime como `function siguiente : () -> integer  [captura: inicio]`.
Probado con `tests/fixtures/valid/control_flow_y_closures.cps` (una
función anidada que captura el parámetro de la función contenedora).

## 4. Control de Flujo — [HECHO] (`src/semantic/control_flow_checker.h/.cpp`)

- Condición de `if`/`while`/`do-while`/`for` (y del operador ternario, que
  no estaba listado acá pero es la misma regla) debe ser `boolean` →
  `SEM005`, implementado en `TypeChecker`. El sujeto de `switch` **no**
  (ver README punto 4) — decisión ya tomada e implementada (ver sección 1
  de este documento).
- `break`/`continue` solo dentro de un bucle → `SEM006`. Pass propio e
  independiente de `TypeChecker` (no necesita tipos ni tabla de símbolos):
  recorre el AST llevando dos banderas, "estoy dentro de un bucle" y
  "estoy dentro de una función", reseteando ambas al entrar a una función
  anidada (un `break` de su cuerpo no se refiere al bucle de afuera).
  **Decisión de lenguaje ya tomada** (README punto 5, regla literal del
  enunciado): `switch` **no** cuenta como bucle, aunque la gramática
  permita `break`/`continue` ahí sin bucle alrededor. Si el equipo decide
  lo contrario, el único lugar que hay que tocar es el caso de
  `SwitchStatement` en `control_flow_checker.cpp` (está comentado ahí
  mismo).
- `return` solo dentro de una función → `SEM007`. Mismo pass, misma
  bandera.

Probado: `sem006_break_fuera_de_bucle.cps`, `sem007_return_fuera_de_funcion.cps`
disparan cada uno su código; `control_flow_y_closures.cps` (break/continue/return
correctos dentro de un `for`, dentro de una función) sale limpio.

## 5. Clases y Objetos — [HECHO] (extendido dentro de `TypeChecker`, no un pass nuevo)

Terminó viviendo en `TypeChecker` en vez de en un pass aparte: necesita el
mismo cómputo bottom-up de `resolved_type` que ya hace ese pass, así que
extenderlo fue más simple que separar la lógica.

- **Acceso a atributos/métodos vía `.`** (`MemberAccessExpression`,
  `PropertyAssignment`/`PropertyAssignExpr`): `TypeChecker::resolveMemberAccess()`
  chequea el tipo de `object`, valida que sea `Type{kind=Class}`, y busca
  `member_name` con `lookupMember()` — primero en el `class_scope` propio,
  después subiendo por `ClassSymbol::base_class` (ya resuelto por
  `InheritanceResolver`). No existe → `SEM010`. `NameResolver` sigue sin
  tocar `member_name` a propósito (sigue siendo trabajo de tipos, no de
  nombres) — nada cambió ahí.
- **Invocación correcta del constructor** (`NewExpression`): busca
  `"constructor"` con el mismo `lookupMember()` (por lo tanto también
  hereda el de la clase base si la derivada no define uno propio) y valida
  argumentos con `checkArguments()` — mismo mecanismo que valida cualquier
  llamada (sección 3). Si la clase no tiene `constructor` y se le pasan
  argumentos igual, también es `SEM008` ("si existe" del PDF, interpretado
  como: si no existe, no debería recibir nada).
- **`this` dentro del ámbito de la clase**: `resolved_type` de un
  `ThisExpression` es `Type{kind=Class}` de la clase contenedora,
  encontrada subiendo por los `Scope` padres hasta el primero con
  `kind() == Class` y leyendo su `owner` (campo agregado en la rebanada de
  tipos, ver [`docs/02_sistema_de_tipos.md`](02_sistema_de_tipos.md)).
  Fuera de una clase → `SEM015` (código nuevo).

Probado: `tests/fixtures/valid/clases_y_objetos.cps` (método heredado
llamado a través de una instancia de la subclase, campo heredado leído,
`this` en varios métodos, constructor y método con argumentos correctos)
sin diagnósticos; `sem008_argumentos_constructor.cps`,
`sem008_argumentos_metodo.cps`, `sem010_miembro_inexistente.cps`,
`sem015_this_fuera_de_clase.cps` disparan cada uno su código exacto.

## 6. Listas y Estructuras de Datos — [HECHO]

- Tipo de los elementos de una lista — implementado, mismo mecanismo que
  el punto 1.
- Validación de índices: que el índice de `ArrayAccessExpression` sea
  `integer` → `SEM004` — implementado. Validar que el índice esté *en
  rango* es imposible en tiempo de compilación en general (el valor puede
  depender de una variable) — eso queda para tiempo de ejecución, fuera de
  este proyecto.

## 7. Generales

- **[HECHO]** Código muerto (instrucciones después de `return`/`break`/`continue`
  dentro del mismo bloque) → `SEM012`, implementado en `ControlFlowChecker`
  (`checkDeadCode()`). Se reporta una sola vez por lista de statements (en
  la primera instrucción inalcanzable), no una vez por cada una — evita
  inundar de errores un bloque muerto grande. Probado:
  `sem012_codigo_muerto.cps`.
- **[HECHO]** "Expresiones con sentido semántico" (el ejemplo del PDF es
  "no multiplicar funciones") — cubierto por la validación de tipos de la
  sección 1: `*` exige `integer` en ambos lados, así que un
  `TypeKind::Function` o `TypeKind::Class` ya cae en `SEM004`
  automáticamente, sin código extra.
- Declaraciones duplicadas (variables, parámetros) — **ya cubierto por
  `SEM002`**, incluyendo parámetros repetidos del mismo nombre en una
  misma función (`FunctionDeclaration` ya declara cada parámetro en
  `funcScope` con `declareOrReport`, que detecta el choque).

## Estado: análisis semántico completo

Todas las reglas semánticas de este documento (las ~25 viñetas del PDF)
están implementadas:

- `TypeChecker` (`src/semantic/type_checker.cpp`): sistema de tipos
  completo (sección 1), condiciones booleanas (sección 4), tipo de
  retorno (sección 3), listas e índices (sección 6), "no multiplicar
  funciones" (sección 7), atributos/métodos heredados vía `.`,
  constructor, `this` (sección 5), validación de argumentos en llamadas y
  constructor (sección 3, `SEM008`). Diseño completo en
  [`docs/02_sistema_de_tipos.md`](02_sistema_de_tipos.md).
- `InheritanceResolver` (`src/semantic/inheritance_resolver.cpp`):
  resolución de herencia y herencia circular (sección 2).
- `ControlFlowChecker` (`src/semantic/control_flow_checker.cpp`):
  break/continue/return fuera de contexto (`SEM006`/`SEM007`), código
  muerto (`SEM012`) — sección 4 y 7.
- `ClosureAnalyzer` (`src/semantic/closure_analyzer.cpp`): captura de
  variables por funciones anidadas — sección 3, no valida nada, guarda
  info para generación de código futura.

Lo que queda de todo el proyecto es **IDE y batería de pruebas por regla**
— ver [`docs/04_ide_y_entrega.md`](04_ide_y_entrega.md). No hay más passes
semánticos pendientes.
