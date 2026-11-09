// Prueba estrella C: base y dos derivadas con override. Las variables y el
// parametro son del tipo base, pero cada llamada ejecuta el salario() de la
// clase real del objeto (vtable + jalr), incluso desde un metodo heredado
// de la base (anual).
// expect-out:
// 1000
// 1500
// 500
// 18000
// 6000
// Gus
class Empleado {
  let nombre: string;
  let base: integer;
  function constructor(nombre: string, base: integer) {
    this.nombre = nombre;
    this.base = base;
  }
  function salario(): integer {
    return this.base;
  }
  function anual(): integer {
    return this.salario() * 12;
  }
}
class Gerente : Empleado {
  let bono: integer = 500;
  function salario(): integer {
    return this.base + this.bono;
  }
}
class Becario : Empleado {
  function salario(): integer {
    return this.base / 2;
  }
}
function pagar(e: Empleado): integer {
  return e.salario();
}
let e1: Empleado = new Empleado("Eva", 1000);
let e2: Empleado = new Gerente("Gus", 1000);
let e3: Empleado = new Becario("Bea", 1000);
print(pagar(e1));
print(pagar(e2));
print(pagar(e3));
print(e2.anual());
print(e3.anual());
print(e2.nombre);
