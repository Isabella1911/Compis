// Error semantico dentro de una funcion: no debe generarse TAC.
// expect: SEM003
function f(n: integer): integer {
  let s: string = n;
  return n;
}
print(f(1));
