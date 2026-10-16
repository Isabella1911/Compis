// while, do-while y for, con break y continue. continue va a la
// condicion en do-while y al update en for.
let i: integer = 0;
let suma: integer = 0;
while (i < 10) {
  i = i + 1;
  if (i % 2 == 0) {
    continue;
  }
  if (i > 7) {
    break;
  }
  suma = suma + i;
}
do {
  i = i - 1;
} while (i > 0);
for (let j: integer = 0; j < 5; j = j + 1) {
  if (j == 1) {
    continue;
  }
  suma = suma + j;
}
print(suma);
