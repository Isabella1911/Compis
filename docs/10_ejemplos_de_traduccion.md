# Ejemplos de traducción

> **Generado** por `tools/gen_ejemplos_doc.py` a partir de `tests/tac/`. No editar a mano: cambiar el caso de prueba, correr `make test-tac-update`, revisar el diff y volver a generar este archivo.
>
> Cada ejemplo es un caso de la batería de pruebas: el programa `.cps` y el TAC que produce `./compiscript --tac` (idéntico al `.tac` esperado). Las reglas de traducción, el formato de cada instrucción y los supuestos están en [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md); aquí solo se muestran los resultados.

## Índice

1. [Expresiones aritméticas](#1-expresiones-aritméticas)
2. [Declaraciones y asignaciones](#2-declaraciones-y-asignaciones)
3. [Strings](#3-strings)
4. [Expresiones lógicas y comparaciones (cortocircuito)](#4-expresiones-lógicas-y-comparaciones-cortocircuito)
5. [`if` / `else`](#5-if--else)
6. [`while`, `do-while`, `for`, `break` y `continue`](#6-while-do-while-for-break-y-continue)
7. [`switch`](#7-switch)
8. [Ternario](#8-ternario)
9. [`try` / `catch`](#9-try--catch)
10. [Temporales con reciclaje](#10-temporales-con-reciclaje)
11. [Funciones, llamadas, `return` y recursión](#11-funciones-llamadas-return-y-recursión)
12. [Funciones anidadas y closures](#12-funciones-anidadas-y-closures)
13. [Clases, `new`, constructor, atributos, métodos y `this`](#13-clases-new-constructor-atributos-métodos-y-this)
14. [Herencia](#14-herencia)
15. [Polimorfismo y despacho dinámico (vtables)](#15-polimorfismo-y-despacho-dinámico-vtables)
16. [Listas e índices](#16-listas-e-índices)
17. [`foreach`](#17-foreach)
18. [`return` dentro de un `try`](#18-return-dentro-de-un-try)
19. [Programa completo](#19-programa-completo)

## 1. Expresiones aritméticas

Precedencia y asociatividad: * / % antes que + -, y a la izquierda.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#101-expresiones-aritméticas). Caso: `tests/tac/p1_aritmetica.cps`.

```cps
let a: integer = 7;
let b: integer = 3;
let c: integer = 2;
print(a + b * c);
print(a - b - c);
print((a - b) * c);
print(a / b % c);
print(-a + b);
```

```
global g_a, 4
global g_b, 4
global g_c, 4

func main, frame=0, temps=1
    g_a = 7
    g_b = 3
    g_c = 2
    t0 = g_b * g_c
    t0 = g_a + t0
    print_int t0
    t0 = g_a - g_b
    t0 = t0 - g_c
    print_int t0
    t0 = g_a - g_b
    t0 = t0 * g_c
    print_int t0
    t0 = g_a / g_b
    t0 = t0 % g_c
    print_int t0
    t0 = -g_a
    t0 = t0 + g_b
    print_int t0
endfunc
```

## 2. Declaraciones y asignaciones

let/var/const, sin inicializador (vale 0), asignacion encadenada y sombreado en un bloque del nivel superior (g_x y g_x_1).

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#102-declaraciones-y-asignaciones). Caso: `tests/tac/p1_declaraciones.cps`.

```cps
let x: integer = 10;
var y: integer;
const LIMITE: integer = 100;
let z: integer = 0;
y = x + 1;
z = y = x * 2;
if (x > 0) {
  let x: integer = 5;
  print(x);
}
print(x);
print(y);
print(z);
print(LIMITE);
```

```
global g_x, 4
global g_y, 4
global g_LIMITE, 4
global g_z, 4
global g_x_1, 4

func main, frame=0, temps=1
    g_x = 10
    g_y = 0
    g_LIMITE = 100
    g_z = 0
    g_y = g_x + 1
    g_y = g_x * 2
    g_z = g_y
    ifFalse g_x > 0 goto L0
    g_x_1 = 5
    print_int g_x_1
L0:
    print_int g_x
    print_int g_y
    print_int g_z
    print_int g_LIMITE
endfunc
```

## 3. Strings

Literales en la tabla de strings (los repetidos comparten etiqueta) y operaciones de contenido delegadas al runtime (__concat, __str_eq).

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#103-strings). Caso: `tests/tac/p1_strings.cps`.

```cps
let nombre: string = "Compi";
let saludo: string = "hola " + nombre;
let igual: boolean = saludo == "hola Compi";
let distinto: boolean = nombre != "Compi";
print(saludo);
print(igual);
print(distinto);
print("hola ");
print(null);
```

```
global g_nombre, 4
global g_saludo, 4
global g_igual, 4
global g_distinto, 4
string str_0, "Compi"
string str_1, "hola "
string str_2, "hola Compi"
string str_3, "null"

func main, frame=0, temps=1
    g_nombre = str_0
    param g_nombre
    param str_1
    g_saludo = call __concat, 2
    param str_2
    param g_saludo
    g_igual = call __str_eq, 2
    param str_0
    param g_nombre
    t0 = call __str_eq, 2
    g_distinto = !t0
    print_str g_saludo
    print_bool g_igual
    print_bool g_distinto
    print_str str_1
    print_str str_3
endfunc
```

## 4. Expresiones lógicas y comparaciones (cortocircuito)

&& y || con cortocircuito: en condiciones se traducen a saltos, y como valor se materializan en 1 o 0. ! intercambia los destinos.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#104-expresiones-lógicas-código-con-saltos-y-cortocircuito). Caso: `tests/tac/p1_logicos.cps`.

```cps
let a: integer = 5;
let b: integer = 10;
let enRango: boolean = a > 0 && a < b;
let alguno: boolean = a == 0 || b == 0;
let ninguno: boolean = !alguno;
if (a < b && (b < 20 || !enRango)) {
  print("si");
}
if (!(a == b)) {
  print("distintos");
}
if (true) {
  print(enRango);
}
print(ninguno);
```

```
global g_a, 4
global g_b, 4
global g_enRango, 4
global g_alguno, 4
global g_ninguno, 4
string str_0, "si"
string str_1, "distintos"

func main, frame=0, temps=1
    g_a = 5
    g_b = 10
    ifFalse g_a > 0 goto L0
    ifFalse g_a < g_b goto L0
    t0 = 1
    goto L1
L0:
    t0 = 0
L1:
    g_enRango = t0
    if g_a == 0 goto L4
    ifFalse g_b == 0 goto L2
L4:
    t0 = 1
    goto L3
L2:
    t0 = 0
L3:
    g_alguno = t0
    g_ninguno = !g_alguno
    ifFalse g_a < g_b goto L5
    if g_b < 20 goto L6
    if g_enRango goto L5
L6:
    print_str str_0
L5:
    if g_a == g_b goto L7
    print_str str_1
L7:
    print_bool g_enRango
L8:
    print_bool g_ninguno
endfunc
```

## 5. `if` / `else`

if sin else, if/else, y else que contiene otro if.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#105-if--else). Caso: `tests/tac/p1_if_else.cps`.

```cps
let nota: integer = 75;
if (nota >= 90) {
  print("excelente");
}
if (nota >= 60) {
  print("aprobado");
} else {
  print("reprobado");
}
if (nota < 60) {
  print("F");
} else {
  if (nota < 80) {
    print("C");
  } else {
    print("A");
  }
}
```

```
global g_nota, 4
string str_0, "excelente"
string str_1, "aprobado"
string str_2, "reprobado"
string str_3, "F"
string str_4, "C"
string str_5, "A"

func main, frame=0, temps=0
    g_nota = 75
    ifFalse g_nota >= 90 goto L0
    print_str str_0
L0:
    ifFalse g_nota >= 60 goto L2
    print_str str_1
    goto L1
L2:
    print_str str_2
L1:
    ifFalse g_nota < 60 goto L4
    print_str str_3
    goto L3
L4:
    ifFalse g_nota < 80 goto L6
    print_str str_4
    goto L5
L6:
    print_str str_5
L5:
L3:
endfunc
```

## 6. `while`, `do-while`, `for`, `break` y `continue`

while, do-while y for, con break y continue. continue va a la condicion en do-while y al update en for.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#106-bucles-break-y-continue). Caso: `tests/tac/p1_bucles.cps`.

```cps
let i: integer = 0;
let suma: integer = 0;
while (i < 10) {
  i = i + 1;
  if (i % 2 == 0) {
    continue;
  }
  if (i > 7) {
    break;
  }
  suma = suma + i;
}
do {
  i = i - 1;
} while (i > 0);
for (let j: integer = 0; j < 5; j = j + 1) {
  if (j == 1) {
    continue;
  }
  suma = suma + j;
}
print(suma);
```

```
global g_i, 4
global g_suma, 4
global g_j, 4

func main, frame=0, temps=1
    g_i = 0
    g_suma = 0
L0:
    ifFalse g_i < 10 goto L1
    g_i = g_i + 1
    t0 = g_i % 2
    ifFalse t0 == 0 goto L2
    goto L0
L2:
    ifFalse g_i > 7 goto L3
    goto L1
L3:
    g_suma = g_suma + g_i
    goto L0
L1:
L4:
    g_i = g_i - 1
L5:
    if g_i > 0 goto L4
L6:
    g_j = 0
L7:
    ifFalse g_j < 5 goto L9
    ifFalse g_j == 1 goto L10
    goto L8
L10:
    g_suma = g_suma + g_j
L8:
    g_j = g_j + 1
    goto L7
L9:
    print_int g_suma
endfunc
```

## 7. `switch`

switch sin fallthrough (cada case termina solo), con default, y switch sobre string (compara contenido con __str_eq).

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#107-switch). Caso: `tests/tac/p1_switch.cps`.

```cps
let dia: integer = 3;
switch (dia) {
  case 1:
    print("lunes");
  case 2:
    print("martes");
  case 3:
    print("miercoles");
  default:
    print("otro");
}
let color: string = "rojo";
switch (color) {
  case "rojo":
    print("alto");
  case "verde":
    print("siga");
}
```

```
global g_dia, 4
global g_color, 4
string str_0, "lunes"
string str_1, "martes"
string str_2, "miercoles"
string str_3, "otro"
string str_4, "rojo"
string str_5, "verde"
string str_6, "alto"
string str_7, "siga"

func main, frame=0, temps=1
    g_dia = 3
    if g_dia == 1 goto L1
    if g_dia == 2 goto L2
    if g_dia == 3 goto L3
    goto L4
L1:
    print_str str_0
    goto L0
L2:
    print_str str_1
    goto L0
L3:
    print_str str_2
    goto L0
L4:
    print_str str_3
L0:
    g_color = str_4
    param str_4
    param g_color
    t0 = call __str_eq, 2
    if t0 goto L6
    param str_5
    param g_color
    t0 = call __str_eq, 2
    if t0 goto L7
    goto L5
L6:
    print_str str_6
    goto L5
L7:
    print_str str_7
    goto L5
L5:
endfunc
```

## 8. Ternario

Ternario: las dos ramas escriben el mismo temporal.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#108-ternario). Caso: `tests/tac/p1_ternario.cps`.

```cps
let a: integer = 4;
let b: integer = 9;
let mayor: integer = a > b ? a : b;
let texto: string = mayor == b ? "b gana" : "a gana";
print(mayor);
print(texto);
print(a < b && b < 10 ? 1 : 0);
```

```
global g_a, 4
global g_b, 4
global g_mayor, 4
global g_texto, 4
string str_0, "b gana"
string str_1, "a gana"

func main, frame=0, temps=1
    g_a = 4
    g_b = 9
    ifFalse g_a > g_b goto L0
    t0 = g_a
    goto L1
L0:
    t0 = g_b
L1:
    g_mayor = t0
    ifFalse g_mayor == g_b goto L2
    t0 = str_0
    goto L3
L2:
    t0 = str_1
L3:
    g_texto = t0
    print_int g_mayor
    print_str g_texto
    ifFalse g_a < g_b goto L4
    ifFalse g_b < 10 goto L4
    t0 = 1
    goto L5
L4:
    t0 = 0
L5:
    print_int t0
endfunc
```

## 9. `try` / `catch`

try/catch: try_begin registra el handler del catch y try_end lo quita. Un break que sale de un try cierra antes su handler.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#109-try--catch). Caso: `tests/tac/p1_try_catch.cps`.

```cps
let intentos: integer = 0;
try {
  intentos = intentos + 1;
} catch (err) {
  print(err);
}
while (true) {
  try {
    intentos = intentos + 1;
    if (intentos > 3) {
      break;
    }
  } catch (e) {
    print("error: " + e);
  }
}
print(intentos);
```

```
global g_intentos, 4
global g_err, 4
global g_e, 4
string str_0, "error: "

func main, frame=0, temps=1
    g_intentos = 0
    try_begin L0
    g_intentos = g_intentos + 1
    try_end
    goto L1
L0:
    g_err = call __error_message, 0
    print_str g_err
L1:
L2:
    try_begin L4
    g_intentos = g_intentos + 1
    ifFalse g_intentos > 3 goto L6
    try_end
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
    print_int g_intentos
endfunc
```

## 10. Temporales con reciclaje

Reciclaje de temporales: cada operando se libera en cuanto se consume, asi que la expresion completa usa solo 2 temporales (temps=2).

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#8-temporales-asignación-y-reciclaje). Caso: `tests/tac/p1_temporales.cps`.

```cps
let a: integer = 1;
let b: integer = 2;
let c: integer = 3;
let d: integer = 4;
let e: integer = 5;
let f: integer = 6;
let r: integer = a * b + c * d - e * f;
let s: integer = (a + b) * (c + d) * (e + f);
print(r + s);
```

```
global g_a, 4
global g_b, 4
global g_c, 4
global g_d, 4
global g_e, 4
global g_f, 4
global g_r, 4
global g_s, 4

func main, frame=0, temps=2
    g_a = 1
    g_b = 2
    g_c = 3
    g_d = 4
    g_e = 5
    g_f = 6
    t0 = g_a * g_b
    t1 = g_c * g_d
    t0 = t0 + t1
    t1 = g_e * g_f
    g_r = t0 - t1
    t0 = g_a + g_b
    t1 = g_c + g_d
    t0 = t0 * t1
    t1 = g_e + g_f
    g_s = t0 * t1
    t0 = g_r + g_s
    print_int t0
endfunc
```

## 11. Funciones, llamadas, `return` y recursión

Recursion: factorial.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#111-funciones-y-return). Caso: `tests/tac/p2_recursion.cps`.

```cps
function fact(n: integer): integer {
  if (n <= 1) {
    return 1;
  }
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

## 12. Funciones anidadas y closures

Funcion anidada que captura un parametro de la contenedora.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#112-closures-funciones-anidadas). Caso: `tests/tac/p2_closure.cps`.

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
func main, frame=0, temps=1
    param 5
    t0 = call fn_contador, 1
    print_int t0
endfunc

func fn_contador_siguiente, frame=0, temps=2
    t0 = fp[4]
    t1 = t0[8]
    t0 = t1 + 1
    return t0
endfunc

func fn_contador, frame=0, temps=1
    link fp
    t0 = call fn_contador_siguiente, 0
    return t0
endfunc
```

## 13. Clases, `new`, constructor, atributos, métodos y `this`

Clase con constructor, atributo y metodo.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#113-clases-new-y-this). Caso: `tests/tac/p2_clase.cps`.

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
print(p.x);
```

```
global g_p, 4
vtable Punto_vtable, Punto_suma

func main, frame=0, temps=1
    t0 = alloc 12
    t0[0] = Punto_vtable
    t0[4] = 0
    t0[8] = 0
    param 4
    param 3
    param t0
    call Punto_constructor, 3
    g_p = t0
    param g_p
    t0 = call Punto_suma, 1
    print_int t0
    t0 = g_p[4]
    print_int t0
endfunc

func Punto_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = x
    t0 = fp[8]
    t0[8] = y
endfunc

func Punto_suma, frame=0, temps=2
    t0 = fp[8]
    t0 = t0[4]
    t1 = fp[8]
    t1 = t1[8]
    t0 = t0 + t1
    return t0
endfunc
```

## 14. Herencia

Herencia: atributo en la base, metodo en la derivada.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#114-herencia). Caso: `tests/tac/p2_herencia.cps`.

```cps
class Animal {
  let nombre: string;

  function constructor(nombre: string) {
    this.nombre = nombre;
  }

  function hablar(): string {
    return this.nombre;
  }
}

class Perro : Animal {
  function ladrar(): string {
    return this.nombre;
  }
}

let p: Perro = new Perro("Toby");
print(p.hablar());
print(p.ladrar());
```

```
global g_p, 4
string str_0, "Toby"
vtable Animal_vtable, Animal_hablar
vtable Perro_vtable, Animal_hablar, Perro_ladrar

func main, frame=0, temps=1
    t0 = alloc 8
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
endfunc

func Animal_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = nombre
endfunc

func Animal_hablar, frame=0, temps=1
    t0 = fp[8]
    t0 = t0[4]
    return t0
endfunc

func Perro_ladrar, frame=0, temps=1
    t0 = fp[8]
    t0 = t0[4]
    return t0
endfunc
```

## 15. Polimorfismo y despacho dinámico (vtables)

Polimorfismo: un objeto de la subclase guardado como la clase base. Los metodos redefinidos se llaman por la vtable del objeto (despacho dinamico); los demas, directo por su etiqueta.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#117-polimorfismo-y-despacho-dinámico). Caso: `tests/tac/p2_polimorfismo.cps`.

```cps
class Figura {
  let lados: integer;
  function constructor(lados: integer) {
    this.lados = lados;
  }
  function area(): integer {
    return 0;
  }
  function describir(): integer {
    return this.lados * 100 + this.area();
  }
}

class Cuadrado : Figura {
  let lado: integer;
  function constructor(lado: integer) {
    this.lados = 4;
    this.lado = lado;
  }
  function area(): integer {
    return this.lado * this.lado;
  }
}

function medir(f: Figura): integer {
  return f.area();
}

let f: Figura = new Cuadrado(3);
print(f.area());
print(f.describir());
print(medir(new Figura(3)));
```

```
global g_f, 4
vtable Figura_vtable, Figura_area, Figura_describir
vtable Cuadrado_vtable, Cuadrado_area, Figura_describir

func main, frame=0, temps=1
    t0 = alloc 12
    t0[0] = Cuadrado_vtable
    t0[4] = 0
    t0[8] = 0
    param 3
    param t0
    call Cuadrado_constructor, 2
    g_f = t0
    param g_f
    t0 = g_f[0]
    t0 = t0[0]
    t0 = call t0, 1
    print_int t0
    param g_f
    t0 = call Figura_describir, 1
    print_int t0
    t0 = alloc 8
    t0[0] = Figura_vtable
    t0[4] = 0
    param 3
    param t0
    call Figura_constructor, 2
    param t0
    t0 = call fn_medir, 1
    print_int t0
endfunc

func Figura_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = lados
endfunc

func Figura_area, frame=0, temps=0
    return 0
endfunc

func Figura_describir, frame=0, temps=2
    t0 = fp[8]
    t0 = t0[4]
    t0 = t0 * 100
    t1 = fp[8]
    param t1
    t1 = t1[0]
    t1 = t1[0]
    t1 = call t1, 1
    t0 = t0 + t1
    return t0
endfunc

func Cuadrado_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = 4
    t0 = fp[8]
    t0[8] = lado
endfunc

func Cuadrado_area, frame=0, temps=2
    t0 = fp[8]
    t0 = t0[8]
    t1 = fp[8]
    t1 = t1[8]
    t0 = t0 * t1
    return t0
endfunc

func fn_medir, frame=0, temps=1
    param f
    t0 = f[0]
    t0 = t0[0]
    t0 = call t0, 1
    return t0
endfunc
```

## 16. Listas e índices

Lista: literal, lectura y escritura por indice.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#115-listas-y-foreach). Caso: `tests/tac/p2_lista.cps`.

```cps
let nums: integer[] = [10, 20, 30];
print(nums[1]);
nums[1] = 99;
print(nums[1]);
```

```
global g_nums, 4
string str_0, "indice fuera de rango"

func main, frame=0, temps=2
    t0 = alloc 16
    t0[0] = 3
    t0[4] = 10
    t0[8] = 20
    t0[12] = 30
    g_nums = t0
    t0 = g_nums[0]
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
    t0 = g_nums[0]
    if 1 < 0 goto L3
    if 1 < t0 goto L2
L3:
    param str_0
    call __raise, 1
L2:
    t0 = 1 * 4
    t0 = t0 + 4
    g_nums[t0] = 99
    t0 = g_nums[0]
    if 1 < 0 goto L5
    if 1 < t0 goto L4
L5:
    param str_0
    call __raise, 1
L4:
    t0 = 1 * 4
    t0 = t0 + 4
    t1 = g_nums[t0]
    print_int t1
endfunc
```

## 17. `foreach`

foreach sobre una lista.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#115-listas-y-foreach). Caso: `tests/tac/p2_foreach.cps`.

```cps
let nums: integer[] = [1, 2, 3];
let total: integer = 0;
foreach (n in nums) {
  total = total + n;
}
print(total);
```

```
global g_nums, 4
global g_total, 4
global g_n, 4

func main, frame=0, temps=4
    t0 = alloc 16
    t0[0] = 3
    t0[4] = 1
    t0[8] = 2
    t0[12] = 3
    g_nums = t0
    g_total = 0
    t0 = g_nums[0]
    t1 = 0
L0:
    ifFalse t1 < t0 goto L2
    t2 = t1 * 4
    t2 = t2 + 4
    g_n = g_nums[t2]
    g_total = g_total + g_n
L1:
    t1 = t1 + 1
    goto L0
L2:
    print_int g_total
endfunc
```

## 18. `return` dentro de un `try`

return dentro de un try: debe emitir try_end antes del return.

Reglas: [`08_lenguaje_intermedio.md`](08_lenguaje_intermedio.md#116-return-dentro-de-un-try). Caso: `tests/tac/p2_return_try.cps`.

```cps
function busca(xs: integer[]): integer {
  try {
    if (xs[0] == 1) {
      return 1;
    }
  } catch (e) {
    print(e);
  }
  return 0;
}

let xs: integer[] = [1, 2];
print(busca(xs));
```

```
global g_xs, 4
string str_0, "indice fuera de rango"

func main, frame=0, temps=1
    t0 = alloc 12
    t0[0] = 2
    t0[4] = 1
    t0[8] = 2
    g_xs = t0
    param g_xs
    t0 = call fn_busca, 1
    print_int t0
endfunc

func fn_busca, frame=4, temps=2
    try_begin L0
    t0 = xs[0]
    if 0 < 0 goto L4
    if 0 < t0 goto L3
L4:
    param str_0
    call __raise, 1
L3:
    t0 = 0 * 4
    t0 = t0 + 4
    t1 = xs[t0]
    ifFalse t1 == 1 goto L2
    try_end
    return 1
L2:
    try_end
    goto L1
L0:
    e = call __error_message, 0
    print_str e
L1:
    return 0
endfunc
```

## 19. Programa completo

Programa "de clase" que combina todas las construcciones: clases con herencia, recursion, closures, listas, foreach, switch, try/catch y expresiones con temporales.

Caso: `tests/tac/p3_programa_completo.cps`.

```cps
class Figura {
  let nombre: string;
  function constructor(nombre: string) {
    this.nombre = nombre;
  }
  function area(): integer {
    return 0;
  }
  function describir(): string {
    return this.nombre + " calculada";
  }
}

class Rectangulo : Figura {
  let ancho: integer;
  let alto: integer;
  function constructor(ancho: integer, alto: integer) {
    this.nombre = "rectangulo";
    this.ancho = ancho;
    this.alto = alto;
  }
  function area(): integer {
    return this.ancho * this.alto;
  }
}

function fibonacci(n: integer): integer {
  if (n < 2) {
    return n;
  }
  return fibonacci(n - 1) + fibonacci(n - 2);
}

function crearAcumulador(inicio: integer): integer {
  let total: integer = inicio;
  function sumar(x: integer): integer {
    total = total + x;
    return total;
  }
  sumar(5);
  return sumar(10);
}

let r: Rectangulo = new Rectangulo(3, 4);
print(r.describir());

let numeros: integer[] = [1, 2, 3, 4, 5];
let suma: integer = 0;
foreach (n in numeros) {
  if (n % 2 == 0) { continue; }
  suma = suma + n * n;
}
print(suma);

let i: integer = 0;
do {
  print(fibonacci(i));
  i = i + 1;
} while (i < 6);

switch (suma) {
  case 35:
    print("correcto");
  default:
    print("incorrecto");
}

try {
  print(numeros[10]);
} catch (e) {
  print("indice fuera de rango: " + e);
}

print(crearAcumulador(1));
let a: integer = 2;
let b: integer = 3;
print(a * b + (a - b) * (a + b) / 2 >= 0 && !(a == b) ? "si" : "no");
```

```
global g_r, 4
global g_numeros, 4
global g_suma, 4
global g_n, 4
global g_i, 4
global g_e, 4
global g_a, 4
global g_b, 4
string str_0, " calculada"
string str_1, "rectangulo"
string str_2, "correcto"
string str_3, "incorrecto"
string str_4, "indice fuera de rango"
string str_5, "indice fuera de rango: "
string str_6, "si"
string str_7, "no"
vtable Figura_vtable, Figura_area, Figura_describir
vtable Rectangulo_vtable, Rectangulo_area, Figura_describir

func main, frame=0, temps=4
    t0 = alloc 16
    t0[0] = Rectangulo_vtable
    t0[4] = 0
    t0[8] = 0
    t0[12] = 0
    param 4
    param 3
    param t0
    call Rectangulo_constructor, 3
    g_r = t0
    param g_r
    t0 = call Figura_describir, 1
    print_str t0
    t0 = alloc 24
    t0[0] = 5
    t0[4] = 1
    t0[8] = 2
    t0[12] = 3
    t0[16] = 4
    t0[20] = 5
    g_numeros = t0
    g_suma = 0
    t0 = g_numeros[0]
    t1 = 0
L1:
    ifFalse t1 < t0 goto L3
    t2 = t1 * 4
    t2 = t2 + 4
    g_n = g_numeros[t2]
    t2 = g_n % 2
    ifFalse t2 == 0 goto L4
    goto L2
L4:
    t2 = g_n * g_n
    g_suma = g_suma + t2
L2:
    t1 = t1 + 1
    goto L1
L3:
    print_int g_suma
    g_i = 0
L5:
    param g_i
    t0 = call fn_fibonacci, 1
    print_int t0
    g_i = g_i + 1
L6:
    if g_i < 6 goto L5
L7:
    if g_suma == 35 goto L9
    goto L10
L9:
    print_str str_2
    goto L8
L10:
    print_str str_3
L8:
    try_begin L11
    t0 = g_numeros[0]
    if 10 < 0 goto L14
    if 10 < t0 goto L13
L14:
    param str_4
    call __raise, 1
L13:
    t0 = 10 * 4
    t0 = t0 + 4
    t1 = g_numeros[t0]
    print_int t1
    try_end
    goto L12
L11:
    g_e = call __error_message, 0
    param g_e
    param str_5
    t0 = call __concat, 2
    print_str t0
L12:
    param 1
    t0 = call fn_crearAcumulador, 1
    print_int t0
    g_a = 2
    g_b = 3
    t0 = g_a * g_b
    t1 = g_a - g_b
    t2 = g_a + g_b
    t1 = t1 * t2
    t1 = t1 / 2
    t0 = t0 + t1
    ifFalse t0 >= 0 goto L15
    if g_a == g_b goto L15
    t0 = str_6
    goto L16
L15:
    t0 = str_7
L16:
    print_str t0
endfunc

func Figura_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = nombre
endfunc

func Figura_area, frame=0, temps=0
    return 0
endfunc

func Figura_describir, frame=0, temps=1
    t0 = fp[8]
    t0 = t0[4]
    param str_0
    param t0
    t0 = call __concat, 2
    return t0
endfunc

func Rectangulo_constructor, frame=0, temps=1
    t0 = fp[8]
    t0[4] = str_1
    t0 = fp[8]
    t0[8] = ancho
    t0 = fp[8]
    t0[12] = alto
endfunc

func Rectangulo_area, frame=0, temps=2
    t0 = fp[8]
    t0 = t0[8]
    t1 = fp[8]
    t1 = t1[12]
    t0 = t0 * t1
    return t0
endfunc

func fn_fibonacci, frame=0, temps=2
    ifFalse n < 2 goto L0
    return n
L0:
    t0 = n - 1
    param t0
    t0 = call fn_fibonacci, 1
    t1 = n - 2
    param t1
    t1 = call fn_fibonacci, 1
    t0 = t0 + t1
    return t0
endfunc

func fn_crearAcumulador_sumar, frame=0, temps=2
    t0 = fp[4]
    t1 = t0[-8]
    t0 = t1 + x
    t1 = fp[4]
    t1[-8] = t0
    t0 = fp[4]
    t1 = t0[-8]
    return t1
endfunc

func fn_crearAcumulador, frame=4, temps=1
    total = inicio
    param 5
    link fp
    t0 = call fn_crearAcumulador_sumar, 1
    param 10
    link fp
    t0 = call fn_crearAcumulador_sumar, 1
    return t0
endfunc
```

## 20. Programas con errores: no se genera TAC

Si el análisis semántico encuentra cualquier error, el generador no corre: `--tac` imprime solo los diagnósticos y el compilador sale con código 1. Estos casos están en `tests/tac/errores/`.

**`e_break_fuera_de_bucle.cps`** — esperado: SEM006

```cps
let i: integer = 0;
if (i == 0) {
  break;
}
```

**`e_funcion_como_valor.cps`** — esperado: SEM016

```cps
function doble(n: integer): integer {
  return n * 2;
}
let g = doble;
```

**`e_no_declarada_en_metodo.cps`** — esperado: SEM001

```cps
class C {
  let x: integer;
  function get(): integer {
    return y;
  }
}
```

**`e_retorno_y_argumentos.cps`** — esperado: SEM009, SEM008

```cps
function doble(n: integer): integer {
  return "dos";
}
print(doble(1, 2));
```

**`e_sintaxis.cps`** — esperado: SYN001

```cps
let x: integer = (1 + 2;
```

**`e_tipo_en_funcion.cps`** — esperado: SEM003

```cps
function f(n: integer): integer {
  let s: string = n;
  return n;
}
print(f(1));
```
