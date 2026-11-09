// try/catch: try_begin registra el handler del catch y try_end lo quita.
// Un break que sale de un try cierra antes su handler.
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// 4
let intentos: integer = 0;
try {
  intentos = intentos + 1;
} catch (err) {
  print(err);
}
while (true) {
  try {
    intentos = intentos + 1;
    if (intentos > 3) {
      break;
    }
  } catch (e) {
    print("error: " + e);
  }
}
print(intentos);
