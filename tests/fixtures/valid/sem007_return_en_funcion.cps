// Contraparte valida de SEM007: return solo dentro de funciones y metodos.
function doble(n: integer): integer {
  return n * 2;
}
function nada() {
  print("sin valor");
  return;
}
class C {
  function valor(): integer { return 1; }
}
nada();
print(doble(4));
