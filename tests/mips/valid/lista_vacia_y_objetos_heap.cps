// Varias reservas seguidas en el heap, incluidas cadenas de largo impar
// (concatenacion), no deben desalinear las listas que vienen despues.
// expect-out:
// abc
// 7
// 0
let s: string = "a" + "bc";
let xs: integer[] = [7];
let vacia: integer[] = [];
print(s);
print(xs[0]);
let n: integer = 0;
foreach (v in vacia) {
  n = n + 1;
}
print(n);
