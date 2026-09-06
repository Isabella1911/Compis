class Perro : Animal {
  function ladrar(): string {
    return "guau";
  }
}

class Animal {
  let nombre: string;
}

class Cachorro : Perro {
  function jugar(): string {
    return "juega";
  }
}

let c: Cachorro = new Cachorro();
print(c.jugar());
