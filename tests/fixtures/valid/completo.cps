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
  function hablar(): string {
    return this.nombre + " ladra.";
  }
}

function factorial(n: integer): integer {
  if (n <= 1) {
    return 1;
  } else {
    return n * factorial(n - 1);
  }
}

let perro: Perro = new Perro("Toby");
let notas: integer[] = [90, 85, 100];
let matriz: integer[][] = [[1, 2], [3, 4]];

let x = 5;
let y = x = 10;
let etiqueta = x > 5 ? "grande" : "chico";

for (let i: integer = 0; i < 3; i = i + 1) {
  print(matriz[0][i]);
}

foreach (n in notas) {
  if (n < 60) { continue; }
  if (n == 100) { break; }
  print(n);
}

switch (x) {
  case 1:
    print("uno");
  case 2:
    print("dos");
  default:
    print("otro");
}

try {
  let peligro = notas[100];
} catch (err) {
  print("Error atrapado: " + err);
}

print(perro.nombre.length);
