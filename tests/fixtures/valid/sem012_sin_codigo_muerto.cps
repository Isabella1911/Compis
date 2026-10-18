// Contraparte valida de SEM012: return/break/continue son la ultima
// instruccion de su bloque.
function signo(n: integer): integer {
  if (n < 0) {
    return -1;
  }
  print("no negativo");
  return 1;
}
print(signo(5));
