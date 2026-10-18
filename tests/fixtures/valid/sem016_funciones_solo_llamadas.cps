// Contraparte valida de SEM016: funciones y metodos solo se llaman, y las
// clases solo se usan con 'new' o como tipo.
class Saludo {
  function texto(): string { return "hola"; }
}
function doble(n: integer): integer {
  return n * 2;
}
let s: Saludo = new Saludo();
print(doble(doble(2)));
print(s.texto());
