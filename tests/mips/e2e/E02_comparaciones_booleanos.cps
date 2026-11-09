// E02: comparaciones entre enteros y booleanos, ! y operadores logicos
// como valor. print de un boolean muestra true/false.
// expect-out:
// true
// false
// true
// true
// false
// true
// true
// false
// true
let a: integer = 3;
let b: integer = 5;
let t: boolean = true;
let f: boolean = false;
print(a < b);
print(a > b);
print(a <= 3);
print(b >= 5);
print(a == b);
print(a != b);
print(t == !f);
print(t && f);
print(t || f);
