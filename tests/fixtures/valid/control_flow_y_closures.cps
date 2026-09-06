function contador(inicio: integer): integer {
  function siguiente(): integer {
    return inicio + 1;
  }
  return siguiente();
}

function buscar(notas: integer[]): integer {
  for (let i: integer = 0; i < 3; i = i + 1) {
    if (notas[i] == 100) {
      return i;
    }
    if (notas[i] < 0) {
      continue;
    }
    if (notas[i] > 1000) {
      break;
    }
  }
  return -1;
}

let notas: integer[] = [90, 100, 85];
print(buscar(notas));
print(contador(5));
