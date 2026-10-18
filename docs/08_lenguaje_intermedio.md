# Lenguaje intermedio: código de tres direcciones (TAC)

> Especificación del código intermedio que genera `compiscript` (Proyecto 2).
> Es la referencia para los calificadores y para la fase siguiente, que
> traduce este TAC a MIPS y lo corre en MARS.
>
> - Las secciones 1 a 9 son comunes a todo el lenguaje.
> - La sección 10 cubre la traducción de expresiones y control de flujo.
> - La sección 11 cubre funciones, closures, clases y listas.
>
> Todos los ejemplos son **salida real** de `./compiscript --tac`. Los `.cps`
> completos están en `tests/tac/`.

## 1. Dónde está en el pipeline

```
programa.cps
  → ANTLR (lexer + parser) → AST                          src/frontend/, src/ast/
  → 6 pases semánticos → AST anotado + tabla de símbolos  src/semantic/
  → FrameLayout: direcciones y registros de activación    src/codegen/frame_layout.*
  → TacGenerator: AST → TAC                               src/codegen/tac_generator.*, gen_*.cpp
  → printer: TAC → texto                                  src/ir/tac_printer.*
```

**Solo se genera TAC si no hubo ningún error.** Si los hay, el compilador
muestra los diagnósticos con su código, línea y columna, y sale con código 1.

**Cómo verlo:**

| Comando | Qué muestra |
|---|---|
| `./compiscript archivo.cps` | Todo: diagnósticos, AST, tabla de símbolos y TAC. También escribe `output/programa.tac`. |
| `./compiscript --tac archivo.cps` | Solo el TAC, o solo los diagnósticos si hubo errores. Es lo que usan las pruebas. |

Las estructuras en memoria están en `src/ir/tac.h`: `Operand`,
`Instruction`, `TacFunction` y `TacProgram`.

## 2. Supuestos de diseño

El diseño se eligió para que la fase de MIPS sea casi una traducción línea
por línea.

1. **Todo valor ocupa una palabra (4 bytes):**
   - `integer` es de 32 bits;
   - `boolean` es 0 o 1;
   - `string`, listas y objetos son **punteros**;
   - `null` es 0.

   Así cada variable, parámetro o atributo cabe en un registro y en una
   casilla de 4 bytes.
2. **Los offsets van en bytes**, como en `lw`/`sw`.
3. **Todos los nombres son identificadores válidos en MIPS** (etiquetas,
   globales y strings). La única excepción es el sufijo `@n` de las locales
   sombreadas (sección 4). Las locales nunca se vuelven etiquetas: en MIPS
   son `offset($fp)`.
4. **Las variables declaradas fuera de toda función son globales
   estáticas**, incluso dentro de un bloque del nivel superior (un `if` o
   `while` fuera de funciones). `main` nunca se reentra, así que no necesita
   registro de activación y su frame es 0.
5. **El orden de evaluación es de izquierda a derecha**, como en TypeScript.
6. **Una variable sin inicializador vale 0**, que es también `false` y
   `null`. Se asigna explícitamente en cada declaración, así una variable
   declarada dentro de un bucle arranca limpia en cada vuelta.
7. **`switch` no tiene fallthrough:** cada `case` termina al llegar al
   siguiente. Es la única lectura coherente con el semántico, que reporta
   `break` dentro de un `switch` como `SEM006` porque `switch` no es un
   bucle. Por lo mismo, un `break` dentro de un `switch` que está dentro de
   un bucle sale del **bucle**.
8. **No hay `throw` en Compiscript.** Lo único que salta a un `catch` son
   los errores de tiempo de ejecución que levanta el runtime (sección 9).
9. **Las operaciones sobre el contenido de strings van al runtime:**
   concatenación (`+`) e igualdad (`==`, `!=`). Un string es un puntero, así
   que comparar o concatenar no es una sola instrucción.
10. **`print` elige la variante según el tipo estático:** `print_int`,
    `print_str` o `print_bool`. `print(null)` imprime el texto `null`. Para
    listas y objetos se imprime su dirección, porque Compiscript no tiene
    conversión de objetos a texto.

## 3. Estructura de un programa TAC

```
global g_<nombre>, <bytes>         ← una por variable global, en orden de declaración
string str_<n>, "<texto>"          ← una por literal de texto distinto
vtable <Clase>_vtable, <m0>, <m1>…  ← una por clase: etiqueta del método de cada slot

func main, frame=<bytes>, temps=<n>
    <instrucciones>                ← el código del nivel superior
endfunc

func <etiqueta>, frame=<bytes>, temps=<n>
    <instrucciones>
endfunc
...
```

- **`main` va siempre primero.** Después vienen las funciones y métodos, en
  el orden en que se terminan de generar.
- **`frame`:** los bytes de variables locales del registro de activación.
- **`temps`:** el máximo de temporales vivos a la vez dentro de esa función.
  Sirve para dimensionar el área de temporales y es la métrica del reciclaje
  (sección 8).
- **Formato:** las etiquetas van en la columna 0; el resto, indentado.
  `# texto` es un comentario.

## 4. Operandos

| Forma | Qué es |
|---|---|
| `t0`, `t1`, … | Temporal. Se reciclan; ver la sección 8. |
| `g_<nombre>` | Variable global. Si está sombreada: `g_x`, `g_x_1`, … |
| `<nombre>` | Variable local o parámetro de la función actual. Si está sombreada dentro de la misma función: `x`, `x@1`, … |
| `42`, `-1` | Constante entera. `true` = `1`, `false` = `0`, `null` = `0`. |
| `str_<n>` | Dirección de un literal de texto. Los literales iguales comparten etiqueta. |
| `fp` | Base del registro de activación actual. Se usa para los enlaces de acceso. |
| `<Clase>_vtable` | Dirección de la tabla de métodos de una clase (sección 11.7). |

La tabla de símbolos dice dónde vive cada nombre (sección 7). En la fase de
MIPS, un `x` local se convierte en `offset($fp)` y un `g_x` en su etiqueta
de `.data`.

## 5. Instrucciones

`x`, `y` y `z` son operandos. `L` es una etiqueta.

| Instrucción | Significado | En MIPS (orientativo) |
|---|---|---|
| `x = y` | copia | `move` / `lw` / `sw` / `li` |
| `x = y op z` | `op` ∈ `+ - * / %`: aritmética entera | `add sub mul div rem` |
| `x = y op z` | `op` ∈ `== != < <= > >=`: deja 1 o 0 | `seq sne slt sle sgt sge` |
| `x = -y` / `x = !y` | negación aritmética / not lógico | `neg` / `xori 1` |
| `L:` | etiqueta | `L:` |
| `goto L` | salto incondicional | `j` |
| `if x goto L` | salta si `x` ≠ 0 | `bnez` |
| `ifFalse x goto L` | salta si `x` = 0 | `beqz` |
| `if x op y goto L` | salta si se cumple la comparación | `beq bne blt ble bgt bge` |
| `ifFalse x op y goto L` | salta si NO se cumple | la comparación opuesta |
| `param x` | argumento para la próxima llamada | push en la pila |
| `link x` | enlace de acceso para la próxima llamada (funciones anidadas) | push en la pila |
| `x = call f, n` / `call f, n` | llama a `f` con `n` parámetros | `jal f`, resultado en `$v0` |
| `x = call y, n` / `call y, n` | llamada **indirecta**: `y` tiene la dirección del método (sacada de una vtable) | `jalr` |
| `return x` / `return` | retorna, con o sin valor | `$v0`, epílogo y `jr $ra` |
| `x = y[k]` | carga la palabra en la dirección `y + k` | `lw` |
| `x[k] = y` | guarda `y` en la dirección `x + k` | `sw` |
| `x = alloc n` | reserva `n` bytes en el heap | `syscall 9` (sbrk) |
| `print_int x` / `print_str x` / `print_bool x` | imprime según el tipo | `syscall 1` / `syscall 4` |
| `try_begin L` | registra un handler: si hay error, salta a `L` | runtime |
| `try_end` | quita el último handler registrado | runtime |

**Convención de llamada:**
- **Argumentos:** se evalúan de izquierda a derecha, pero los `param` se
  emiten de **derecha a izquierda**, como en cdecl. Así el parámetro `i`
  queda en `fp + 8 + 4*i` (sección 6).
- **`this`:** en métodos es el parámetro 0, es decir, el último `param`
  antes del `call`.
- **`n` en `call f, n`:** cuenta todos los `param` de esa llamada, `this`
  incluido.

## 6. Registro de activación

Los offsets son relativos a `fp` y cada casilla mide 4 bytes. Esta
distribución la calcula `src/codegen/frame_layout.cpp`.

```
direcciones altas
    fp + 8 + 4*i   parámetro i       ← 'this' es el parámetro 0 en métodos y constructores
    fp + 4         enlace de acceso  ← static link: frame de la función que CONTIENE a esta
    fp + 0         enlace de control ← fp del llamador
    fp - 4         dirección de retorno ($ra)
    fp - 8 - 4*k   variable local k
    ...            área de temporales (temps × 4 bytes)
direcciones bajas  ← sp
```

**Reparto entre llamador y llamado:**
- El **llamador** empuja los parámetros (`param`) y, si la función es
  anidada, el enlace de acceso (`link`).
- El **llamado** guarda el `fp` anterior y `$ra`, y reserva el espacio de
  locales y temporales.
- El **valor de retorno** viaja en un registro (`$v0`), no en el frame.

**Cómo se reparten las casillas:**
- Cada variable local tiene su propia casilla, en orden de declaración; no
  se reusan casillas entre bloques hermanos.
- Las globales no ocupan frame: viven en el área estática (`global g_x, 4`).

## 7. Tabla de símbolos extendida

Además de nombre, tipo y línea, cada símbolo lleva información para la
generación de código (`src/semantic/symbol.h`):

| Campo | Para quién | Qué es |
|---|---|---|
| `storage` | variables, constantes, parámetros, atributos | `Global`, `Param`, `Local` o `Field` |
| `offset` | `Param` / `Local` / `Field` | bytes relativos a `fp`, o desde el inicio del objeto |
| `size` | todos | 4 |
| `ir_name` | variables | nombre en el TAC (`g_x`, `x@1`) |
| `label` | funciones | `fn_<nombre>`, `fn_<externa>_<interna>`, `<Clase>_<metodo>` |
| `frame_size` | funciones | bytes de locales |
| `nesting_depth` | funciones | 0 = nivel superior o método; +1 por cada función que la contiene |
| `is_method` | funciones | `this` es el parámetro 0 |
| `instance_size` | clases | bytes de un objeto: palabra 0 (puntero a la vtable) + atributos, heredados incluidos |
| `vtable_label`, `vtable` | clases | etiqueta de su tabla de métodos y el método de cada slot |
| `vtable_slot` | métodos | posición en la vtable (la misma en todas las subclases) |
| `overridden` | métodos | alguna subclase lo redefine: se llama por la vtable |

`./compiscript archivo.cps` la imprime con esa información, ordenada por
línea:

```
Scope(Global)
  var x : integer  (linea 1)  [global g_x, 4 bytes]
  function externa : (a: integer, b: integer) -> integer  (linea 6)  [fn_externa, frame 12 bytes, anidamiento 0]
  Scope(Block)
    var x : integer  (linea 3)  [global g_x_1, 4 bytes]
  Scope(Function)
    param a : integer  (linea 6)  [param fp+8, 4 bytes]
    param b : integer  (linea 6)  [param fp+12, 4 bytes]
    var x : integer  (linea 7)  [local fp-8, 4 bytes]
    function interna : () -> integer  [captura: a]  (linea 12)  [fn_externa_interna, frame 4 bytes, anidamiento 1]
    Scope(Block)
      var x : integer  (linea 9)  [local fp-12, 4 bytes, en TAC x@1]
  Scope(Class)
    function ladrar : (veces: integer) -> string  (linea 23)  [Perro_ladrar, frame 4 bytes, anidamiento 0, metodo]
    Scope(Function)
      param veces : integer  (linea 23)  [param fp+12, 4 bytes]
```

Se nota que en el método `ladrar` el primer parámetro está en `fp+12`,
porque `this` ocupa `fp+8`.

**Cómo usa la tabla cada fase:**

| Fase | Qué hace con la tabla |
|---|---|
| `DeclarationCollector` | La construye. |
| `NameResolver` y `TypeChecker` | La consultan y completan tipos. |
| `ClosureAnalyzer` | Anota las capturas. |
| `FrameLayout` | Le agrega direcciones y frames. |
| `TacGenerator` | La consulta para cada variable (`storage`, `ir_name`) y cada llamada (`label`). |
| Fase de MIPS (siguiente) | La consultará para traducir cada nombre a `offset($fp)` o a una etiqueta. |

## 8. Temporales: asignación y reciclaje

**El algoritmo** está en `src/codegen/temp_allocator.*`:

- Un temporal guarda un valor intermedio que se **consume exactamente una
  vez**.
- Al consumirlo, quien lo usó lo libera (`release`) y vuelve a la lista de
  libres.
- `acquire()` entrega siempre **el libre de número más bajo**.
- Los operandos se liberan **antes** de pedir el temporal del resultado. Así
  el resultado reusa el temporal de un operando (`t0 = t0 + t1`), lo cual es
  seguro porque la instrucción lee antes de escribir.
- Hay un allocator por función: cada `func` vuelve a empezar en `t0`.

**Resultado:** una expresión usa tantos temporales como valores intermedios
estén vivos **a la vez**, no uno por operación.

```cps
let r: integer = a * b + c * d - e * f;
let s: integer = (a + b) * (c + d) * (e + f);
```

```
func main, frame=0, temps=2
    t0 = g_a * g_b
    t1 = g_c * g_d
    t0 = t0 + t1          ← t0 y t1 se liberan y el resultado reusa t0
    t1 = g_e * g_f
    g_r = t0 - t1
    t0 = g_a + g_b
    t1 = g_c + g_d
    t0 = t0 * t1
    t1 = g_e + g_f
    g_s = t0 * t1
```

Son 10 operaciones con solo 2 temporales (`temps=2`). Sin reciclaje serían
8.

**Optimización al asignar** (`assignTo`): si el valor a guardar es el
temporal que acaba de calcular la instrucción anterior, se reescribe el
destino de esa instrucción en lugar de emitir una copia. Por eso aparece
`g_r = t0 - t1` y no `t1 = t0 - t1` seguido de `g_r = t1`. Es seguro porque
ese temporal no lo lee nadie más.

## 9. Rutinas del runtime

Se llaman con la convención normal (`param` de derecha a izquierda y luego
`call`). Se implementan en MIPS en la fase siguiente.

| Rutina | Parámetros | Devuelve | Para qué |
|---|---|---|---|
| `__concat` | `a`, `b` (strings) | string nuevo `a + b` | `+` entre strings |
| `__str_eq` | `a`, `b` (strings) | 1 si el contenido es igual | `==`, `!=` y `switch` entre strings |
| `__raise` | mensaje (string) | no vuelve | Levanta un error de tiempo de ejecución: salta al `catch` del último `try_begin` activo, o aborta si no hay ninguno. |
| `__error_message` | — | el mensaje del último error | Lo llama el `catch` para llenar su variable. |

## 10. Traducción de expresiones y control de flujo

### 10.1 Expresiones aritméticas

La precedencia y la asociatividad ya vienen en la forma del AST: los
operadores se pliegan a la izquierda y `* / %` quedan por debajo de `+ -`.
El generador recorre el árbol en postorden.

```cps
print(a + b * c);
print(a - b - c);
print(a / b % c);
print(-a + b);
```

```
    t0 = g_b * g_c
    t0 = g_a + t0
    print_int t0
    t0 = g_a - g_b        ← (a - b) - c: a la izquierda
    t0 = t0 - g_c
    print_int t0
    t0 = g_a / g_b
    t0 = t0 % g_c
    print_int t0
    t0 = -g_a
    t0 = t0 + g_b
    print_int t0
```

### 10.2 Declaraciones y asignaciones

```cps
let x: integer = 10;
var y: integer;
const LIMITE: integer = 100;
z = y = x * 2;
if (x > 0) {
  let x: integer = 5;
  print(x);
}
```

```
    g_x = 10
    g_y = 0               ← sin inicializador: 0
    g_LIMITE = 100        ← const: el semántico ya impide reasignarla
    g_y = g_x * 2         ← asignación encadenada: el valor de (y = v) es y
    g_z = g_y
    ifFalse g_x > 0 goto L0
    g_x_1 = 5             ← el x del bloque sombrea al global: otro nombre
    print_int g_x_1
L0:
```

### 10.3 Strings

```cps
let nombre: string = "Compi";
let saludo: string = "hola " + nombre;
let distinto: boolean = nombre != "Compi";
```

```
string str_0, "Compi"
string str_1, "hola "

    g_nombre = str_0
    param g_nombre        ← los param van de derecha a izquierda
    param str_1
    g_saludo = call __concat, 2
    param str_0           ← "Compi" repetido: se reusa str_0
    param g_nombre
    t0 = call __str_eq, 2
    g_distinto = !t0
```

### 10.4 Expresiones lógicas: código con saltos y cortocircuito

`genCond(expr, V, F)` genera código que salta a `V` si la condición es
verdadera y a `F` si es falsa. Una etiqueta vacía significa "seguir de
largo" en ese caso. Así nunca se emite un salto a la instrucción siguiente.

| Expresión | Traducción |
|---|---|
| `a op b`, con `op` relacional | Un solo salto: `if a op b goto V` o `ifFalse a op b goto F`. No se calcula un booleano intermedio. |
| `A && B` | `genCond(A, "", F')` y luego `genCond(B, V, F)`. Si `A` es falsa, `B` no se evalúa. |
| `A \|\| B` | `genCond(A, V', "")` y luego `genCond(B, V, F)`. Si `A` es verdadera, `B` no se evalúa. |
| `!A` | `genCond(A, F, V)`: se intercambian las salidas y no se emite ninguna instrucción. |
| `true` / `false` | `goto V` / `goto F`, o nada si esa salida sigue de largo. |
| otro valor | `if x goto V` / `ifFalse x goto F`. |

`F'` y `V'` son etiquetas nuevas, que solo se crean si `F` o `V` venían
vacías.

**En una condición** (`if`, `while`, …):

```cps
if (a < b && (b < 20 || !enRango)) { print("si"); }
```

```
    ifFalse g_a < g_b goto L5     ← a < b falso: no se evalúa nada más
    if g_b < 20 goto L6           ← b < 20 verdadero: no se evalúa !enRango
    if g_enRango goto L5          ← !enRango: salida invertida
L6:
    print_str str_0
L5:
```

**Como valor** (`let ok = a > 0 && a < b;`), se usa el mismo código con
saltos y el resultado se guarda como 1 o 0:

```
    ifFalse g_a > 0 goto L0
    ifFalse g_a < g_b goto L0
    t0 = 1
    goto L1
L0:
    t0 = 0
L1:
    g_enRango = t0
```

### 10.5 `if` / `else`

```
genCond(cond, "", Lelse)          genCond(cond, "", Lend)
<then>                            <then>
goto Lend                       Lend:
Lelse:
<else>                            (sin else)
Lend:
```

```cps
if (nota >= 60) { print("aprobado"); } else { print("reprobado"); }
```

```
    ifFalse g_nota >= 60 goto L2
    print_str str_1
    goto L1
L2:
    print_str str_2
L1:
```

### 10.6 Bucles, `break` y `continue`

Cada bucle registra dos etiquetas en una pila: a dónde va `break` y a dónde
va `continue`. Los dos van al bucle más interno.

| Bucle | Esquema | `continue` va a |
|---|---|---|
| `while` | `Lbegin: genCond(c, "", Lend)` `<cuerpo>` `goto Lbegin` `Lend:` | `Lbegin` |
| `do-while` | `Lbegin: <cuerpo>` `Lcond: genCond(c, Lbegin, "")` `Lend:` | `Lcond` |
| `for` | `<init>` `Lbegin: genCond(c, "", Lend)` `<cuerpo>` `Lupd: <update>` `goto Lbegin` `Lend:` | `Lupd` |

Un `for` sin condición, o un `while (true)`, no emite salto de salida: es un
bucle infinito del que solo se sale con `break`.

```cps
while (i < 10) {
  i = i + 1;
  if (i % 2 == 0) { continue; }
  if (i > 7) { break; }
  suma = suma + i;
}
for (let j: integer = 0; j < 5; j = j + 1) {
  if (j == 1) { continue; }
  suma = suma + j;
}
```

```
L0:
    ifFalse g_i < 10 goto L1
    g_i = g_i + 1
    t0 = g_i % 2
    ifFalse t0 == 0 goto L2
    goto L0               ← continue
L2:
    ifFalse g_i > 7 goto L3
    goto L1               ← break
L3:
    g_suma = g_suma + g_i
    goto L0
L1:
    ...
    g_j = 0
L7:
    ifFalse g_j < 5 goto L9
    ifFalse g_j == 1 goto L10
    goto L8               ← continue en for: va al update
L10:
    g_suma = g_suma + g_j
L8:
    g_j = g_j + 1
    goto L7
L9:
```

### 10.7 `switch`

El sujeto se evalúa **una sola vez**. Después van todas las comparaciones y
luego los cuerpos, cada uno terminado en `goto Lend` (sin fallthrough, ver
supuesto 7). Si el sujeto es un string, se compara con `__str_eq`.

```cps
switch (dia) {
  case 1: print("lunes");
  case 2: print("martes");
  default: print("otro");
}
```

```
    if g_dia == 1 goto L1
    if g_dia == 2 goto L2
    goto L3               ← ningún case coincidió: default (o Lend si no hay)
L1:
    print_str str_0
    goto L0
L2:
    print_str str_1
    goto L0
L3:
    print_str str_2
L0:
```

### 10.8 Ternario

Las dos ramas escriben **el mismo** temporal, que queda reservado mientras se
evalúa la rama `else`:

```cps
let mayor: integer = a > b ? a : b;
```

```
    ifFalse g_a > g_b goto L0
    t0 = g_a
    goto L1
L0:
    t0 = g_b
L1:
    g_mayor = t0
```

### 10.9 `try` / `catch`

- `try_begin Lcatch` registra el handler y `try_end` lo quita al terminar el
  `try` sin errores.
- Al entrar al `catch`, `__error_message` devuelve el mensaje y se guarda en
  la variable del `catch`, que es de tipo `string`.
- Un `break` o `continue` que sale de un `try` emite antes un `try_end` por
  cada `try` que abandona. Un `return` dentro de un `try` hace lo mismo
  (sección 11). Así nunca queda un handler colgado.

```cps
while (true) {
  try {
    intentos = intentos + 1;
    if (intentos > 3) { break; }
  } catch (e) {
    print("error: " + e);
  }
}
```

```
L2:
    try_begin L4
    g_intentos = g_intentos + 1
    ifFalse g_intentos > 3 goto L6
    try_end               ← el break sale del try: primero cierra su handler
    goto L3
L6:
    try_end
    goto L5
L4:
    g_e = call __error_message, 0
    param g_e
    param str_0
    t0 = call __concat, 2
    print_str t0
L5:
    goto L2
L3:
```

## 11. Funciones, closures, clases y listas

Implementación en `src/codegen/gen_functions.cpp`, `gen_classes.cpp` y
`gen_arrays.cpp`. Los `.cps` de esta sección están en `tests/tac/p2_*.cps`.

### Supuestos

1. **Despacho dinámico con vtables.** Un objeto de una subclase se puede
   guardar donde se espera su clase base (`let a: Animal = new Perro()`),
   así que el método que corre lo decide el objeto. Cada clase tiene una
   vtable y la palabra 0 de cada objeto apunta a la de su clase real. Si un
   método **no** lo redefine ninguna subclase, se llama directo por su
   etiqueta (`call Animal_hablar, 1`); si alguna lo redefine, se llama por
   la vtable (sección 11.7). El constructor nunca pasa por la vtable.
2. **Las funciones no escapan.** No se soporta `let f = g;` ni pasar una
   función como valor: el semántico lo rechaza con `SEM016`, igual que
   llamar a algo que no es función. Solo se llaman por nombre (o como
   método). Una función anidada solo se invoca desde el ámbito que la
   contiene (o desde otra anidada del mismo árbol).
3. **Layout de listas:** en el heap, palabra 0 = longitud; las siguientes
   son los elementos (`[4]`, `[8]`, …). Índice fuera de rango →
   `call __raise, 1` con el mensaje `"indice fuera de rango"`.
4. **Layout de objetos:** palabra 0 = puntero a la vtable; después los
   atributos en orden de declaración, **base primero** (empiezan en `+4`).
   Cada atributo ocupa 4 bytes. `ClassSymbol::instance_size` es el total. El constructor se busca en la clase y en sus bases; se llama
   después de `alloc` y de los inicializadores de atributos.

### 11.1 Funciones y `return`

```cps
function fact(n: integer): integer {
  if (n <= 1) { return 1; }
  return n * fact(n - 1);
}
print(fact(5));
```

```
func main, frame=0, temps=1
    param 5
    t0 = call fn_fact, 1
    print_int t0
endfunc

func fn_fact, frame=0, temps=1
    ifFalse n <= 1 goto L0
    return 1
L0:
    t0 = n - 1
    param t0
    t0 = call fn_fact, 1
    t0 = n * t0
    return t0
endfunc
```

Los argumentos se evalúan de izquierda a derecha y los `param` se emiten
de derecha a izquierda. Un `return` dentro de un `try` emite `try_end`
antes (ver 11.6).

### 11.2 Closures (funciones anidadas)

```cps
function contador(inicio: integer): integer {
  function siguiente(): integer {
    return inicio + 1;
  }
  return siguiente();
}
print(contador(5));
```

```
func fn_contador_siguiente, frame=0, temps=2
    t0 = fp[4]             ← frame de contador (enlace de acceso)
    t1 = t0[8]             ← parametro inicio en ese frame
    t0 = t1 + 1
    return t0
endfunc

func fn_contador, frame=0, temps=1
    link fp                ← al llamar a la anidada, su enlace es este frame
    t0 = call fn_contador_siguiente, 0
    return t0
endfunc
```

- Al llamar a una función con `nesting_depth > 0` se emite `link` con el
  frame de su contenedora directa (subiendo `fp[4]` si hace falta).
- Leer/escribir una variable capturada: `genCapturedLoad` /
  `genCapturedStore` suben `nesting_depth_actual - nesting_depth_dueña`
  veces por `fp[4]` y luego usan `symbol->offset`.

### 11.3 Clases, `new` y `this`

```cps
class Punto {
  let x: integer;
  let y: integer;
  function constructor(x: integer, y: integer) {
    this.x = x;
    this.y = y;
  }
  function suma(): integer {
    return this.x + this.y;
  }
}
let p: Punto = new Punto(3, 4);
print(p.suma());
```

```
vtable Punto_vtable, Punto_suma

func main, frame=0, temps=1
    t0 = alloc 12          ← 4 (vtable) + 2 atributos
    t0[0] = Punto_vtable   ← palabra 0: la vtable de la clase real
    t0[4] = 0              ← inicializador ausente: 0
    t0[8] = 0
    param 4
    param 3
    param t0               ← this
    call Punto_constructor, 3
    g_p = t0
    param g_p
    t0 = call Punto_suma, 1
    print_int t0
endfunc

func Punto_constructor, frame=0, temps=1
    t0 = fp[8]             ← this
    t0[4] = x
    t0 = fp[8]
    t0[8] = y
endfunc
```

`this` es `fp[8]`. En un método, el receptor es el último `param` antes
del `call` (parámetro 0 del frame).

### 11.4 Herencia

```cps
class Animal {
  let nombre: string;
  function constructor(nombre: string) { this.nombre = nombre; }
  function hablar(): string { return this.nombre; }
}
class Perro : Animal {
  function ladrar(): string { return this.nombre; }
}
let p: Perro = new Perro("Toby");
print(p.hablar());
print(p.ladrar());
```

```
vtable Animal_vtable, Animal_hablar
vtable Perro_vtable, Animal_hablar, Perro_ladrar   ← slot 0 heredado, slot 1 propio

    t0 = alloc 8           ← instance_size de Perro = 4 (vtable) + 4 (atributo heredado)
    t0[0] = Perro_vtable
    t0[4] = 0
    param str_0
    param t0
    call Animal_constructor, 2
    g_p = t0
    param g_p
    t0 = call Animal_hablar, 1
    print_str t0
    param g_p
    t0 = call Perro_ladrar, 1
    print_str t0
```

El atributo `nombre` queda en `+4` tanto en `Animal` como en `Perro`, y
`hablar` ocupa el slot 0 en las dos vtables. Como ninguna subclase redefine
`hablar` ni `ladrar`, se llaman directo por su etiqueta.

### 11.5 Listas y `foreach`

```cps
let nums: integer[] = [10, 20, 30];
print(nums[1]);
nums[1] = 99;
```

```
    t0 = alloc 16
    t0[0] = 3
    t0[4] = 10
    t0[8] = 20
    t0[12] = 30
    g_nums = t0
    t0 = g_nums[0]         ← longitud
    if 1 < 0 goto L1
    if 1 < t0 goto L0
L1:
    param str_0
    call __raise, 1
L0:
    t0 = 1 * 4
    t0 = t0 + 4
    t1 = g_nums[t0]
    print_int t1
```

`foreach` usa un temporal como índice oculto, `pushLoop` para
`break`/`continue`, y escribe cada elemento en la variable del bucle
(global o local, según el ámbito).

### 11.6 `return` dentro de un `try`

```cps
function busca(xs: integer[]): integer {
  try {
    if (xs[0] == 1) { return 1; }
  } catch (e) {
    print(e);
  }
  return 0;
}
```

```
    try_begin L0
    ...
    ifFalse t1 == 1 goto L2
    try_end                ← cierra el handler antes de salir
    return 1
L2:
    try_end
    goto L1
L0:
    e = call __error_message, 0
    print_str e
L1:
    return 0
```

### 11.7 Polimorfismo y despacho dinámico

Cuando una subclase redefine un método, toda llamada a ese método se hace
por la vtable del objeto:

```
param obj              ← this, como siempre
t = obj[0]             ← la vtable de la clase real del objeto
t = t[4*slot]          ← la dirección del método en ese slot
x = call t, n          ← llamada indirecta
```

```cps
class Figura {
  let lados: integer;
  function constructor(lados: integer) { this.lados = lados; }
  function area(): integer { return 0; }
  function describir(): integer { return this.lados * 100 + this.area(); }
}
class Cuadrado : Figura {
  let lado: integer;
  function constructor(lado: integer) { this.lados = 4; this.lado = lado; }
  function area(): integer { return this.lado * this.lado; }
}
function medir(f: Figura): integer { return f.area(); }
let f: Figura = new Cuadrado(3);
print(f.area());
```

```
vtable Figura_vtable, Figura_area, Figura_describir
vtable Cuadrado_vtable, Cuadrado_area, Figura_describir   ← area redefinida en el slot 0

    t0 = alloc 12
    t0[0] = Cuadrado_vtable    ← f es un Cuadrado aunque su tipo sea Figura
    ...
    param g_f
    t0 = g_f[0]
    t0 = t0[0]                 ← slot 0: Cuadrado_area
    t0 = call t0, 1

func fn_medir, frame=0, temps=1
    param f
    t0 = f[0]
    t0 = t0[0]
    t0 = call t0, 1            ← Figura_area o Cuadrado_area según el objeto
    return t0
endfunc
```

- La vtable de una subclase empieza con los slots de la base, en el mismo
  orden; un método redefinido reemplaza su slot y uno nuevo se agrega al
  final. Por eso `slot` es el mismo para una llamada sin importar la clase
  real del objeto.
- `describir` no se redefine, así que `f.describir()` sigue siendo
  `call Figura_describir, 1`; adentro, `this.area()` sí va por la vtable.
- Reglas de tipos: una subclase se puede asignar, pasar y devolver donde se
  espera su base (`SEM003`/`SEM008`/`SEM009` lo aceptan), pero no al revés.

Caso completo: `tests/tac/p2_polimorfismo.cps`.
