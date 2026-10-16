// try/catch: try_begin registra el handler del catch y try_end lo quita.
// Un break que sale de un try cierra antes su handler.
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
