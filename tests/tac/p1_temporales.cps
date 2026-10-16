// Reciclaje de temporales: cada operando se libera en cuanto se consume,
// asi que la expresion completa usa solo 2 temporales (temps=2).
let a: integer = 1;
let b: integer = 2;
let c: integer = 3;
let d: integer = 4;
let e: integer = 5;
let f: integer = 6;
let r: integer = a * b + c * d - e * f;
let s: integer = (a + b) * (c + d) * (e + f);
print(r + s);
