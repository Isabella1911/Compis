// Listas en el heap: literal, lectura, escritura, foreach, listas anidadas,
// listas de strings y una lista pasada a una funcion.
// expect-out:
// 20
// 99
// 139
// 139
// 6
// 3
// b
// 5
function suma(xs: integer[]): integer {
  let total: integer = 0;
  foreach (x in xs) {
    total = total + x;
  }
  return total;
}
let nums: integer[] = [10, 20, 30];
print(nums[1]);
nums[1] = 99;
print(nums[1]);
let total: integer = 0;
foreach (n in nums) {
  total = total + n;
}
print(total);
print(suma(nums));
let m: integer[][] = [[1, 2], [3]];
print(m[0][0] + m[0][1] + m[1][0]);
print(m[1][0]);
let letras: string[] = ["a", "b", "c"];
print(letras[1]);
print(suma([2, 3]));
