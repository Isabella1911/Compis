// Clase con un error de sintaxis: falta el ';' despues del campo.
// expect: SYN001
class A {
  let x: integer
  function f(): integer {
    return 1;
  }
}
