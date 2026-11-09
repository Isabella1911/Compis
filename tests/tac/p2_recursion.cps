// Recursion: factorial.
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// 120
function fact(n: integer): integer {
  if (n <= 1) {
    return 1;
  }
  return n * fact(n - 1);
}

print(fact(5));
