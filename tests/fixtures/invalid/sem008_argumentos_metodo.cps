class Calculadora {
  function sumar(a: integer, b: integer): integer {
    return a + b;
  }
}

let c: Calculadora = new Calculadora();
print(c.sumar(1));
