// try/catch con errores de runtime reales (no hay throw en Compiscript):
// indice fuera de rango, division entre cero dentro de una funcion,
// error a 50 llamadas de profundidad (se descartan esos frames), try
// anidados, error dentro de un catch (lo atrapa el try de afuera), return
// dentro de un try y acceso a una lista null.
// expect-out:
// capturado: indice fuera de rango
// sin error
// 5
// capturado: division entre cero
// profundo: division entre cero
// interno: indice fuera de rango
// externo: division entre cero
// 1
// 0
// busca: indice fuera de rango
// 0
// capturado: referencia nula
// fin
function divide(a: integer, b: integer): integer {
  return a / b;
}
function profundo(n: integer): integer {
  if (n == 0) {
    return divide(1, 0);
  }
  return profundo(n - 1) + 1;
}
function busca(xs: integer[]): integer {
  try {
    if (xs[0] == 1) {
      return 1;
    }
  } catch (e) {
    print("busca: " + e);
  }
  return 0;
}
let cero: integer = 0;
let xs: integer[] = [1, 2, 3];
try {
  print(xs[10]);
  print("no se imprime");
} catch (e) {
  print("capturado: " + e);
}
try {
  print("sin error");
} catch (e) {
  print("no se imprime");
}
try {
  print(divide(10, 2));
  print(divide(1, 0));
} catch (e) {
  print("capturado: " + e);
}
try {
  print(profundo(50));
} catch (e) {
  print("profundo: " + e);
}
try {
  try {
    print(xs[-1]);
  } catch (e) {
    print("interno: " + e);
    print(10 % cero);
  }
} catch (e) {
  print("externo: " + e);
}
print(busca(xs));
print(busca([5]));
let vacia: integer[] = [];
print(busca(vacia));
let nada: integer[] = null;
try {
  print(nada[0]);
} catch (e) {
  print("capturado: " + e);
}
print("fin");
