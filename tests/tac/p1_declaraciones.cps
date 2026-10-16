// let/var/const, sin inicializador (vale 0), asignacion encadenada y
// sombreado en un bloque del nivel superior (g_x y g_x_1).
let x: integer = 10;
var y: integer;
const LIMITE: integer = 100;
let z: integer = 0;
y = x + 1;
z = y = x * 2;
if (x > 0) {
  let x: integer = 5;
  print(x);
}
print(x);
print(y);
print(z);
print(LIMITE);
