// expect-out:
// 0
// 1
// 2
let i: integer = 0;
while (i < 3 && true) {
  print(i);
  i = i + 1;
}
