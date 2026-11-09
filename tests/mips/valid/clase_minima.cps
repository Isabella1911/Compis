// Una clase sola, sin instancias ni codigo ejecutable: igual debe compilar a
// un .asm valido (main solo termina). Campos con y sin inicializador.
class Punto {
  let x: integer;
  let y: integer = 10;
  let etiqueta: string = "origen";
  function constructor(x: integer) {
    this.x = x;
  }
  function norma1(): integer {
    return this.x + this.y;
  }
}
