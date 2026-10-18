// Varios errores a la vez: se reportan todos y no hay TAC.
// expect: SEM009
// expect: SEM008
function doble(n: integer): integer {
  return "dos";
}
print(doble(1, 2));
