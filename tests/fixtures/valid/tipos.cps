function suma(a: integer, b: integer): integer {
  return a + b;
}

let x: integer = suma(2, 3);
let mensaje: string = "hola " + "mundo";
let ok: boolean = x > 0 && mensaje != "";
let notas: integer[] = [90, 85, 100];

for (let i: integer = 0; i < 3; i = i + 1) {
  print(notas[i]);
}

switch (x) {
  case 1:
    print("uno");
  case 5:
    print("cinco");
  default:
    print("otro");
}

let etiqueta: string = ok ? "si" : "no";
print(etiqueta);
