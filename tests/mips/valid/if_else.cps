// expect-out:
// aprobado
// C
let nota: integer = 75;
if (nota >= 90) {
  print("excelente");
}
if (nota >= 60) {
  print("aprobado");
} else {
  print("reprobado");
}
if (nota < 60) {
  print("F");
} else {
  if (nota < 80) {
    print("C");
  } else {
    print("A");
  }
}
