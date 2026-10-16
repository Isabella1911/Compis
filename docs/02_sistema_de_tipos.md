# Sistema de tipos

> **Actualización: esto ya está implementado** (`src/semantic/type.h/.cpp`,
> `type_checker.h/.cpp`), conectado a `Compiler::compile()` como Pass 3, y
> probado (`make test`, fixtures `tests/fixtures/*/sem00*.cps`). Se deja el
> documento tal como se escribió originalmente (útil como referencia de
> diseño y de las decisiones que se tomaron), con notas `[HECHO]` marcando
> qué quedó resuelto y cómo. Lo que seguía bloqueado por esto — la mayoría
> de [`docs/03_passes_semanticos.md`](03_passes_semanticos.md) — ya se
> puede empezar.

## 0. Dónde estamos parados

Ya existen y están probados (`make test`, `make test-symbols`):

- El AST completo (`src/ast/nodes.h`), con `AstNode::resolved_type` en
  `nullptr` en todos los nodos, esperando esto.
- La tabla de símbolos (`src/semantic/symbol.h`, `scope.h`,
  `symbol_table.h`): `Symbol::declared_type` guarda el tipo **tal como se
  escribió en el código** (`ast::TypeAnnotationPtr` — `NamedTypeAnnotation`
  o `ArrayTypeAnnotation`), no un tipo resuelto.
- `DeclarationCollector` (Pass 1): declara todo en el scope que
  corresponde.
- `NameResolver` (Pass 2): conecta cada uso de un identificador con su
  `Symbol` (`SEM001` si no existe).

Lo que falta es el paso intermedio: convertir esos `TypeAnnotation` (texto)
y esas expresiones (estructura) en un `Type` real con el que se pueda
comparar, y dejarlo pegado en `resolved_type`.

## 1. La clase `Type`

Crear `src/semantic/type.h` (mismo patrón que `symbol.h`):

```cpp
enum class TypeKind { Integer, String, Boolean, Null, Array, Function, Class, Void, Error };

class Type {
public:
    TypeKind kind;
    // Solo aplica si kind == Array:
    std::shared_ptr<Type> element_type;
    // Solo aplica si kind == Class:
    ClassSymbol* class_symbol = nullptr;
    // Solo aplica si kind == Function:
    std::vector<std::shared_ptr<Type>> param_types;
    std::shared_ptr<Type> return_type;

    bool equals(const Type& other) const;   // comparacion estructural
};
```

`TypeKind::Error` es útil como "tipo comodín" para cuando algo ya falló:
así una expresión que arrastra un error no dispara un segundo error en
cascada por cada operación que la usa después (comparar contra `Error`
siempre da "compatible", para cortar el ruido).

**[HECHO] El bug que temíamos, evitado:** `ast/nodes.h` ahora forward-declara
`semantic::Type` (junto a `Symbol`/`Scope`) y `AstNode::resolved_type` es
`std::shared_ptr<semantic::Type>` — con una salvedad respecto a lo que
proponía este documento originalmente: se usa `shared_ptr`, no un puntero
crudo. Razón: muchos `Type` se sintetizan al vuelo durante el chequeo (el
tipo resultado de un `BinaryExpression`, por ejemplo) y no tienen otro
dueño estable — un puntero crudo ahí quedaría colgante apenas el
`shared_ptr` local que lo creó saliera de scope. `shared_ptr<Type>` con
`Type` solo declarado adelante es válido en C++ (el destructor no
necesita la definición completa en `nodes.h`).

## 2. De `TypeAnnotation` a `Type` — [HECHO]

`resolveTypeAnnotation()` en `src/semantic/type.cpp`, con la firma
sugerida acá casi textual. Nombre de clase inválido o no declarado →
`SEM013` (se agregó a `codes.h`, no existía antes de este trabajo).

## 3. Pass 3: `TypeChecker` — [HECHO], con más alcance del que pedía este documento

Terminó siendo, en la práctica, la recomendación de la sección 3 misma:
"en la práctica probablemente conviene que este mismo pass compute el
tipo Y valide en el mismo recorrido" — `TypeChecker`
(`src/semantic/type_checker.h/.cpp`) hace ambas cosas en un solo recorrido,
y ya cubre bastante de
[`docs/03_passes_semanticos.md`](03_passes_semanticos.md) sección 1
(`SEM003`, `SEM004`), la parte de condiciones booleanas de la sección 4
(`SEM005`), tipo de retorno de la sección 3 (`SEM009`), y la
compatibilidad `switch`/`case` que la sección 4 de este documento dejaba
como decisión pendiente. Sigue recorriendo el AST llenando
`resolved_type` en cada nodo relevante:

- **Declaraciones** (`VariableDeclaration`, `ConstantDeclaration`,
  `Parameter`, retorno de `FunctionDeclaration`): si tienen
  `declared_type`, resolverlo con la función de arriba. Si no lo tienen
  (`let x = 5;` sin anotar), el tipo se **infiere** del inicializador —
  necesita que las expresiones ya tengan su propio `resolved_type`, así
  que este pass tiene que resolver expresiones de abajo hacia arriba
  (exactamente como la Etapa 1 explicó sobre atributos sintetizados: el
  tipo de un nodo se calcula desde sus hijos).
- **Expresiones**: acá es donde entra la lógica real —
  - `LiteralExpression` → tipo directo por `LiteralKind`.
  - `IdentifierExpression` → el `resolved_type` de su `Symbol` (que ya
    quedó resuelto en el paso anterior, si el orden de declaración lo
    permite — ver nota sobre orden abajo).
  - `BinaryExpression`, `UnaryExpression` → **[HECHO]** valida y computa
    tipo en el mismo recorrido, tal como sugería este párrafo. `+` acepta
    `integer+integer` o `string+string` (no mezcla, ver decisión de la
    sección 4 más abajo, ya implementada); `- * / %` exigen `integer` en
    ambos lados; `&& ||` exigen `boolean`; `== !=` exigen tipos
    compatibles entre sí (cualquiera); `< <= > >=` exigen `integer`.

### Nota sobre orden de declaración

Compiscript no exige declarar antes de usar dentro de una función (una
función puede llamar a otra declarada más abajo en el mismo archivo — muy
común, sobre todo con recursión mutua). Para variables/constantes esto es
menos común pero la gramática no lo prohíbe. Como `DeclarationCollector`
(Pass 1) ya declaró **todos** los símbolos de un scope antes de que
`TypeAssigner` empiece a resolver ninguno, esto no es problema para
funciones y clases (sus firmas ya existen completas). El único caso
delicado es inferir el tipo de `let x = algo_no_declarado_todavia` dentro
de un mismo bloque secuencial — ahí sí importa el orden textual. No hace
falta resolverlo de entrada; si aparece, es simplemente "usar una variable
antes de inicializarla", un caso borde a decidir en equipo.

## 4. Decisiones de equipo — estado actual

- **[HECHO] `string + integer` no se permite** — `TypeChecker` exige
  `integer+integer` o `string+string` para `+`, cualquier otra combinación
  es `SEM004`. Si el equipo prefiere permitir conversión implícita más
  adelante, es un cambio acotado al bloque `op == "+"` de
  `type_checker.cpp`.
- **Pendiente:** ¿qué tipo tiene un `arrayLiteral` vacío (`[]`)? Hoy
  `TypeChecker` le da `Array(Error)` si no hay elementos (no revienta,
  pero tampoco es un tipo útil para validar después). No hay fixture que
  ejercite este caso todavía.
- **[HECHO] Compatibilidad `switch`/`case`**: el sujeto y cada `case` se
  validan con `equals()` (mismo tipo o compatible), `SEM004` si no —
  implementado en `TypeChecker::checkStatement`, caso `SwitchStatement`.
