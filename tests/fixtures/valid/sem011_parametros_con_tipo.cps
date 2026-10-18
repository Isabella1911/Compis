// Contraparte valida de SEM011: todos los parametros (de funciones,
// metodos y constructores) llevan anotacion de tipo.
class Par {
  let a: integer;
  function constructor(a: integer) { this.a = a; }
  function mas(b: integer): integer { return this.a + b; }
}
function unir(x: string, y: string): string {
  return x + y;
}
print(unir("a", "b"));
print(new Par(1).mas(2));
