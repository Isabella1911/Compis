// Contraparte valida de SEM005: todas las condiciones son booleanas.
let i: integer = 0;
if (i < 3) { print("menor"); }
while (i < 3) { i = i + 1; }
do { i = i - 1; } while (i > 0);
for (let j: integer = 0; j < 2; j = j + 1) { print(j); }
let t: string = i == 0 ? "cero" : "otro";
print(t);
