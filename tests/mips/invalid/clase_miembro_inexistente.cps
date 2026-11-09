// Acceso a un miembro que la clase no tiene.
// expect: SEM010
class A {
  let x: integer;
}
let a: A = new A();
print(a.y);
