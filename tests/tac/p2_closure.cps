// Funcion anidada que captura un parametro de la contenedora.
// Salida al ejecutarlo en MARS (tests/mips/run_mips_tests.py):
// expect-out:
// 6
function contador(inicio: integer): integer {
  function siguiente(): integer {
    return inicio + 1;
  }
  return siguiente();
}

print(contador(5));
