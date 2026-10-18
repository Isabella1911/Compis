// Un programa con varios errores a la vez: el compilador debe reportarlos
// todos (no se detiene en el primero) y no generar TAC.
// expect: SEM001
// expect: SEM003
// expect: SEM005
let x: integer = 1;
x = "texto";
if (x) {
  print(z);
}
