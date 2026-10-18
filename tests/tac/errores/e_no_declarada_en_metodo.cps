// Variable no declarada dentro de un metodo: no debe generarse TAC.
// expect: SEM001
class C {
  let x: integer;
  function get(): integer {
    return y;
  }
}
