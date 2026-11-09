// Un indice invalido detiene el programa con un error de runtime claro
// (el TAC llama a __raise con "indice fuera de rango").
// expect-out:
// 1
// Error en tiempo de ejecucion: indice fuera de rango
let xs: integer[] = [1, 2];
print(xs[0]);
print(xs[5]);
print(99);
