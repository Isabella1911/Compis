// Ternario: las dos ramas escriben el mismo temporal.
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// 9
// b gana
// 1
let a: integer = 4;
let b: integer = 9;
let mayor: integer = a > b ? a : b;
let texto: string = mayor == b ? "b gana" : "a gana";
print(mayor);
print(texto);
print(a < b && b < 10 ? 1 : 0);
