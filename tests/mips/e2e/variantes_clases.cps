// Variantes de la prueba estrella: un metodo declarado antes que los
// atributos y el constructor, cinco atributos (dos con inicializador),
// constructor de tres parametros, objetos como atributos de otro objeto,
// acceso y asignacion encadenados (s.fin.x = 10) y un objeto local dentro
// de una funcion.
// expect-out:
// 9
// p
// 13
// 15
class Punto3D {
  function suma(): integer {
    return this.x + this.y + this.z;
  }
  let x: integer;
  let y: integer;
  let z: integer;
  let w: integer = 7;
  let etiqueta: string = "p";
  function constructor(x: integer, y: integer, z: integer) {
    this.x = x;
    this.y = y;
    this.z = z;
  }
}
class Segmento {
  let inicio: Punto3D;
  let fin: Punto3D;
  function constructor(a: Punto3D, b: Punto3D) {
    this.inicio = a;
    this.fin = b;
  }
  function largoManhattan(): integer {
    return this.fin.suma() - this.inicio.suma();
  }
}
function construirYMedir(n: integer): integer {
  let p: Punto3D = new Punto3D(n, n, n);
  return p.suma() + p.w;
}
let s: Segmento = new Segmento(new Punto3D(1, 2, 3), new Punto3D(4, 5, 6));
print(s.largoManhattan());
print(s.inicio.etiqueta);
print(construirYMedir(2));
s.fin.x = 10;
print(s.largoManhattan());
