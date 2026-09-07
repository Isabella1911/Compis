class A {
 let x: integer=1;
 let y: integer=this.x;
 function constructor(x: integer){this.x=x;}
 function f(): integer {function g(): integer {return this.x;} return g();}
}
let a=new A(2); print(a.f());
