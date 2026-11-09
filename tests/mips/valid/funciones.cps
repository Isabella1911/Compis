// Secuencias de llamada: 0, 1, 4 y 6 argumentos (los 6 van por la pila
// igual que los demas), orden de evaluacion de los argumentos, llamadas
// anidadas, funcion void, recursion directa e indirecta, locales
// independientes en cada nivel, recursion profunda, 5000 llamadas en un
// bucle y return desde dentro de un bucle. Al terminar, main comprueba que
// $sp quedo donde estaba (si no, "pila desbalanceada").
// expect-out:
// 7
// 6
// 1234
// 123456
// 17
// 1
// 2
// 3
// 4
// 1234
// hola
// 55
// true
// true
// 0
// 2
// 4
// 6
// 500500
// 5000
// 5
// -1
// Ana!
function f0(): integer {
  return 7;
}
function f1(a: integer): integer {
  return a + 1;
}
function f4(a: integer, b: integer, c: integer, d: integer): integer {
  return a * 1000 + b * 100 + c * 10 + d;
}
function f6(a: integer, b: integer, c: integer, d: integer, e: integer, f: integer): integer {
  return a * 100000 + b * 10000 + c * 1000 + d * 100 + e * 10 + f;
}
function marca(v: integer): integer {
  print(v);
  return v;
}
function saluda() {
  print("hola");
}
function fib(n: integer): integer {
  if (n < 2) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}
function esPar(n: integer): boolean {
  if (n == 0) {
    return true;
  }
  return esImpar(n - 1);
}
function esImpar(n: integer): boolean {
  if (n == 0) {
    return false;
  }
  return esPar(n - 1);
}
function locales(n: integer) {
  let doble: integer = n * 2;
  if (n > 0) {
    locales(n - 1);
  }
  print(doble);
}
function sumaHasta(n: integer): integer {
  if (n == 0) {
    return 0;
  }
  return n + sumaHasta(n - 1);
}
function primeroMayor(xs: integer[], x: integer): integer {
  foreach (v in xs) {
    if (v > x) {
      return v;
    }
  }
  return -1;
}
function nombre(): string {
  return "Ana";
}

print(f0());
print(f1(5));
print(f4(1, 2, 3, 4));
print(f6(1, 2, 3, 4, 5, 6));
print(f1(f4(0, 0, 1, 6)));
print(f4(marca(1), marca(2), marca(3), marca(4)));
saluda();
print(fib(10));
print(esPar(10));
print(esImpar(7));
locales(3);
print(sumaHasta(1000));
let i: integer = 0;
let acc: integer = 0;
while (i < 5000) {
  acc = acc + f1(i) - i;
  i = i + 1;
}
print(acc);
print(primeroMayor([1, 5, 9], 4));
print(primeroMayor([1, 5, 9], 10));
print(nombre() + "!");
