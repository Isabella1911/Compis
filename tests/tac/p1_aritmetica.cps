// Precedencia y asociatividad: * / % antes que + -, y a la izquierda.
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// 13
// 2
// 8
// 0
// -4
let a: integer = 7;
let b: integer = 3;
let c: integer = 2;
print(a + b * c);
print(a - b - c);
print((a - b) * c);
print(a / b % c);
print(-a + b);
