// Herencia en tres niveles, campo heredado con inicializador, constructor
// heredado y propio, override y despacho dinamico por la vtable: el metodo
// de la base presentar() llama a this.sonido(), y cada objeto ejecuta el
// sonido de su clase real aunque la variable sea del tipo base.
// expect-out:
// Toby dice guau
// Michi dice miau
// Bolt dice guau chiquito
// Piolin dice ...
// Algo dice ...
// miau
// 2
// 4
// guau chiquito
class Animal {
  let nombre: string;
  let patas: integer = 4;
  function constructor(nombre: string) {
    this.nombre = nombre;
  }
  function sonido(): string {
    return "...";
  }
  function presentar(): string {
    return this.nombre + " dice " + this.sonido();
  }
}
class Perro : Animal {
  function sonido(): string {
    return "guau";
  }
}
class Gato : Animal {
  function sonido(): string {
    return "miau";
  }
}
class Cachorro : Perro {
  function sonido(): string {
    return "guau chiquito";
  }
}
class Pajaro : Animal {
  function constructor(nombre: string) {
    this.nombre = nombre;
    this.patas = 2;
  }
}
function hazSonar(a: Animal): string {
  return a.sonido();
}
let a1: Animal = new Perro("Toby");
let a2: Animal = new Gato("Michi");
let a3: Animal = new Cachorro("Bolt");
let a4: Animal = new Pajaro("Piolin");
let a5: Animal = new Animal("Algo");
print(a1.presentar());
print(a2.presentar());
print(a3.presentar());
print(a4.presentar());
print(a5.presentar());
let x: Animal = new Gato("Tom");
print(x.sonido());
print(a4.patas);
print(a1.patas);
print(hazSonar(a3));
