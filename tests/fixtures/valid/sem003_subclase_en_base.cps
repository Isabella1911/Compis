// Contraparte valida de SEM003/SEM008/SEM009 con herencia: un objeto de la
// subclase se puede guardar, pasar y devolver donde se espera la base.
class Animal {
  function hablar(): string { return "..."; }
}
class Perro : Animal {
  function hablar(): string { return "guau"; }
}
function voz(a: Animal): string {
  return a.hablar();
}
function crear(): Animal {
  return new Perro();
}
let a: Animal = new Perro();
a = crear();
print(voz(new Perro()));
print(a == crear());
