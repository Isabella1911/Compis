// Contraparte valida de SEM015: 'this' solo dentro de metodos de clase.
class Contador {
  let n: integer;
  function constructor() { this.n = 0; }
  function incrementar(): integer {
    this.n = this.n + 1;
    return this.n;
  }
}
let k: Contador = new Contador();
print(k.incrementar());
