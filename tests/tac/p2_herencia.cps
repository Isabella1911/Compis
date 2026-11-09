// Herencia: atributo en la base, metodo en la derivada.
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// Toby
// Toby
class Animal {
  let nombre: string;

  function constructor(nombre: string) {
    this.nombre = nombre;
  }

  function hablar(): string {
    return this.nombre;
  }
}

class Perro : Animal {
  function ladrar(): string {
    return this.nombre;
  }
}

let p: Perro = new Perro("Toby");
print(p.hablar());
print(p.ladrar());
