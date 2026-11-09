// Clase con campos (con y sin inicializador), constructor y metodos.
// Dos objetos independientes, metodo que llama a otro metodo, metodo que
// llama a una funcion, retorno de this y acceso encadenado.
// expect-out:
// 1
// 110
// 3
// 110
// 6
// 3
// 50
// contador
// 0
function duplicar(x: integer): integer {
  return x * 2;
}
class Contador {
  let valor: integer;
  let paso: integer;
  let nombre: string = "contador";
  let extra: integer;
  function constructor(inicio: integer, paso: integer) {
    this.valor = inicio;
    this.paso = paso;
  }
  function avanzar(): integer {
    this.valor = this.valor + this.paso;
    return this.valor;
  }
  function avanzarDos(): integer {
    this.avanzar();
    return this.avanzar();
  }
  function doble(): integer {
    return duplicar(this.valor);
  }
  function mismo(): Contador {
    return this;
  }
}
let a: Contador = new Contador(0, 1);
let b: Contador = new Contador(100, 10);
print(a.avanzar());
print(b.avanzar());
print(a.avanzarDos());
print(b.valor);
print(a.doble());
print(a.mismo().valor);
let c: Contador = a.mismo();
c.valor = 50;
print(a.valor);
print(a.nombre);
print(b.extra);
