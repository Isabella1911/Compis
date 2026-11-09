// Prueba estrella D: una clase con errores independientes en lineas
// distintas. El compilador los reporta todos (no se detiene en el primero)
// y no deja ningun .asm.
// expect: SEM013
// expect: SEM002
// expect: SEM010
class Producto {
  let precio: integer;
  let categoria: Categoria;
  let precio: integer;
  function total(): integer {
    return this.precio * this.cantidad;
  }
}
