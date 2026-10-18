// expect: SEM016
class C {
  function f(): integer { return 1; }
}
let c: C = new C();
let m = c.f;
