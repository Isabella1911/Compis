// Contraparte valida de SEM004: aritmetica entre enteros, logica entre
// booleanos, comparaciones del mismo tipo y concatenacion string + string.
let a: integer = 6;
let b: integer = 4;
let suma: integer = a + b * 2 - a / b % 3;
let ok: boolean = a > b && !(a == b) || false;
let msg: string = "suma" + " = ok";
print(msg);
print(ok);
