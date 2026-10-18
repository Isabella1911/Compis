// Contraparte valida de SEM003: asignaciones con tipos compatibles
// (mismo tipo, o null en un tipo de referencia).
class Animal {
  let nombre: string;
}
class Perro : Animal {
}
let x: integer = 5;
x = 10;
let s: string = "hola";
s = s + " mundo";
let a: Animal = new Animal();
let p: Perro = null;
p = new Perro();
let lista: integer[] = [1, 2, 3];
lista[0] = 7;
