// E04: && y || con efectos laterales. marca() imprime su nombre: si un
// operando no hace falta, no se evalua y su nombre no aparece (x, y, w).
// Tanto en condiciones (saltos) como guardado en una variable.
// expect-out:
// a
// b
// fin1
// c
// d
// false
// e
// f
// true
// g
// false
function marca(nombre: string, valor: boolean): boolean {
  print(nombre);
  return valor;
}
if (marca("a", false) && marca("x", true)) {
  print("no");
}
if (marca("b", true) || marca("y", true)) {
  print("fin1");
}
let r: boolean = marca("c", true) && marca("d", false);
print(r);
let q: boolean = marca("e", false) || marca("f", true);
print(q);
let z: boolean = marca("g", false) && marca("w", true);
print(z);
