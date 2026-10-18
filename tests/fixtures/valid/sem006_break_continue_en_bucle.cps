// Contraparte valida de SEM006: break y continue dentro de bucles.
let i: integer = 0;
while (true) {
  i = i + 1;
  if (i == 2) { continue; }
  if (i > 4) { break; }
  print(i);
}
for (let k: integer = 0; k < 10; k = k + 1) {
  if (k == 3) { break; }
}
