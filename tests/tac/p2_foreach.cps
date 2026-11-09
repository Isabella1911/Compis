// foreach sobre una lista.
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// 6
let nums: integer[] = [1, 2, 3];
let total: integer = 0;
foreach (n in nums) {
  total = total + n;
}
print(total);
