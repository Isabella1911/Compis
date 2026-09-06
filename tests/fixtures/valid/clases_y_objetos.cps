class Animal {
  let nombre: string;

  function constructor(nombre: string) {
    this.nombre = nombre;
  }

  function hablar(): string {
    return this.nombre + " hace ruido.";
  }
}

class Perro : Animal {
  function ladrar(): string {
    return this.nombre + " ladra.";
  }
}

class Calculadora {
  function sumar(a: integer, b: integer): integer {
    return a + b;
  }
}

let p: Perro = new Perro("Toby");
print(p.hablar());
print(p.ladrar());
print(p.nombre);

let c: Calculadora = new Calculadora();
print(c.sumar(2, 3));
