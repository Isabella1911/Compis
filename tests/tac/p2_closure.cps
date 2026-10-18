// Funcion anidada que captura un parametro de la contenedora.
function contador(inicio: integer): integer {
  function siguiente(): integer {
    return inicio + 1;
  }
  return siguiente();
}

print(contador(5));
