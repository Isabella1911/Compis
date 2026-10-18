// Polimorfismo: un objeto de la subclase guardado como la clase base. Los
// metodos redefinidos se llaman por la vtable del objeto (despacho
// dinamico); los demas, directo por su etiqueta.
class Figura {
  let lados: integer;
  function constructor(lados: integer) {
    this.lados = lados;
  }
  function area(): integer {
    return 0;
  }
  function describir(): integer {
    return this.lados * 100 + this.area();
  }
}

class Cuadrado : Figura {
  let lado: integer;
  function constructor(lado: integer) {
    this.lados = 4;
    this.lado = lado;
  }
  function area(): integer {
    return this.lado * this.lado;
  }
}

function medir(f: Figura): integer {
  return f.area();
}

let f: Figura = new Cuadrado(3);
print(f.area());
print(f.describir());
print(medir(new Figura(3)));
