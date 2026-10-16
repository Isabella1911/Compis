// Ternario: las dos ramas escriben el mismo temporal.
let a: integer = 4;
let b: integer = 9;
let mayor: integer = a > b ? a : b;
let texto: string = mayor == b ? "b gana" : "a gana";
print(mayor);
print(texto);
print(a < b && b < 10 ? 1 : 0);
