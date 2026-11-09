// Clase con constructor, atributo y metodo.
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// 7
// 3
class Punto {
  let x: integer;
  let y: integer;

  function constructor(x: integer, y: integer) {
    this.x = x;
    this.y = y;
  }

  function suma(): integer {
    return this.x + this.y;
  }
}

let p: Punto = new Punto(3, 4);
print(p.suma());
print(p.x);
