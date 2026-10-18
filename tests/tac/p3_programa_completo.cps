// Programa "de clase" que combina todas las construcciones: clases con
// herencia, recursion, closures, listas, foreach, switch, try/catch y
// expresiones con temporales.
class Figura {
  let nombre: string;
  function constructor(nombre: string) {
    this.nombre = nombre;
  }
  function area(): integer {
    return 0;
  }
  function describir(): string {
    return this.nombre + " calculada";
  }
}

class Rectangulo : Figura {
  let ancho: integer;
  let alto: integer;
  function constructor(ancho: integer, alto: integer) {
    this.nombre = "rectangulo";
    this.ancho = ancho;
    this.alto = alto;
  }
  function area(): integer {
    return this.ancho * this.alto;
  }
}

function fibonacci(n: integer): integer {
  if (n < 2) {
    return n;
  }
  return fibonacci(n - 1) + fibonacci(n - 2);
}

function crearAcumulador(inicio: integer): integer {
  let total: integer = inicio;
  function sumar(x: integer): integer {
    total = total + x;
    return total;
  }
  sumar(5);
  return sumar(10);
}

let r: Rectangulo = new Rectangulo(3, 4);
print(r.describir());

let numeros: integer[] = [1, 2, 3, 4, 5];
let suma: integer = 0;
foreach (n in numeros) {
  if (n % 2 == 0) { continue; }
  suma = suma + n * n;
}
print(suma);

let i: integer = 0;
do {
  print(fibonacci(i));
  i = i + 1;
} while (i < 6);

switch (suma) {
  case 35:
    print("correcto");
  default:
    print("incorrecto");
}

try {
  print(numeros[10]);
} catch (e) {
  print("indice fuera de rango: " + e);
}

print(crearAcumulador(1));
let a: integer = 2;
let b: integer = 3;
print(a * b + (a - b) * (a + b) / 2 >= 0 && !(a == b) ? "si" : "no");
