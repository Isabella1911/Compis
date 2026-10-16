// Precedencia y asociatividad: * / % antes que + -, y a la izquierda.
let a: integer = 7;
let b: integer = 3;
let c: integer = 2;
print(a + b * c);
print(a - b - c);
print((a - b) * c);
print(a / b % c);
print(-a + b);
