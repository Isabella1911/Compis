// Contraparte valida de SEM010: acceso a atributos y metodos que existen,
// incluidos los heredados.
class Animal {
  let nombre: string;
  function constructor(nombre: string) { this.nombre = nombre; }
  function hablar(): string { return this.nombre; }
}
class Perro : Animal {
  let edad: integer;
}
let p: Perro = new Perro("Toby");
p.edad = 3;
print(p.nombre);
print(p.hablar());
print(p.edad);
