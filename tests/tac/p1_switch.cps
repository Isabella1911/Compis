// switch sin fallthrough (cada case termina solo), con default, y switch
// sobre string (compara contenido con __str_eq).
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// miercoles
// alto
let dia: integer = 3;
switch (dia) {
  case 1:
    print("lunes");
  case 2:
    print("martes");
  case 3:
    print("miercoles");
  default:
    print("otro");
}
let color: string = "rojo";
switch (color) {
  case "rojo":
    print("alto");
  case "verde":
    print("siga");
}
