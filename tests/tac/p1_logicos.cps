// && y || con cortocircuito: en condiciones se traducen a saltos, y como
// valor se materializan en 1 o 0. ! intercambia los destinos.
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
