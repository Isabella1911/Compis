// Un try que se cierra con break no deja su handler activo: el error que
// viene despues, fuera de todo try, termina el programa (codigo 1) con el
// mensaje claro en lugar de saltar a un catch viejo.
// expect-exit: 1
// expect-out:
// 3
// Error en tiempo de ejecucion: division entre cero
let i: integer = 0;
let cero: integer = 0;
while (true) {
  try {
    i = i + 1;
    if (i == 3) {
      break;
    }
  } catch (e) {
    print("no se imprime");
  }
}
print(i);
print(1 / cero);
print("no se imprime");
