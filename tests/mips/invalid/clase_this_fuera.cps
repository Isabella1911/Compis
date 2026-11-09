// this fuera de un metodo.
// expect: SEM015
class A {
  let x: integer;
}
function f(): integer {
  return this.x;
}
