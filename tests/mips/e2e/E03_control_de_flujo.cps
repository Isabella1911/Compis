// E03: if/else, for, while, do-while y switch (sin fallthrough, con
// default) sobre una expresion.
// expect-out:
// grande
// 15
// 10
// 3
// martes
// otro
let n: integer = 12;
if (n > 10) {
  print("grande");
} else {
  print("chico");
}
let s: integer = 0;
for (let i: integer = 1; i <= 5; i = i + 1) {
  s = s + i;
}
print(s);
let w: integer = 0;
while (w < 10) {
  w = w + 2;
}
print(w);
let k: integer = 0;
do {
  k = k + 1;
} while (k < 3);
print(k);
let d: integer = 2;
switch (d) {
  case 1:
    print("lunes");
  case 2:
    print("martes");
  default:
    print("otro");
}
switch (d + 5) {
  case 1:
    print("lunes");
  default:
    print("otro");
}
