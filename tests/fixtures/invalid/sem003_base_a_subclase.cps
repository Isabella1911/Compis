// La herencia solo permite subclase -> base, no al reves.
// expect: SEM003
class Animal {
  let nombre: string;
}
class Perro : Animal {
}
let a: Animal = new Animal();
let p: Perro = a;
