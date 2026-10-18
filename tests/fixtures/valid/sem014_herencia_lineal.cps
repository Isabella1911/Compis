// Contraparte valida de SEM014: cadena de herencia sin ciclos.
class A {
  let x: integer;
}
class B : A {
  let y: integer;
}
class C : B {
  let z: integer;
}
let c: C = new C();
c.x = 1;
print(c.x);
