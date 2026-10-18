// Contraparte valida de SEM008: numero y tipo de argumentos correctos en
// funciones, constructores y metodos.
class Calculadora {
  let base: integer;
  function constructor(base: integer) {
    this.base = base;
  }
  function sumar(a: integer, b: integer): integer {
    return this.base + a + b;
  }
}
function saludar(nombre: string): string {
  return "hola " + nombre;
}
let c: Calculadora = new Calculadora(10);
print(c.sumar(1, 2));
print(saludar("Ana"));
