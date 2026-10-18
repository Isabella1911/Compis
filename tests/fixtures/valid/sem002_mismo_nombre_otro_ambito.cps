// Contraparte valida de SEM002: el mismo nombre en ambitos distintos no es
// redeclaracion (la variable interna sombrea a la externa).
let x: integer = 5;
{
  let x: string = "otro ambito";
  print(x);
}
function f(x: integer): integer {
  return x;
}
print(f(x));
