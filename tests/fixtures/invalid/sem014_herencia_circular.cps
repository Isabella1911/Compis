// Se reporta una vez por cada clase del ciclo.
// expect: SEM014
// expect: SEM014
class A : B {
  let x: integer;
}

class B : A {
  let y: integer;
}
