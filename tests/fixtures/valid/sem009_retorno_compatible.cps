// Contraparte valida de SEM009: el valor de cada return coincide con el
// tipo declarado de la funcion.
function nombre(): string {
  return "Ana";
}
function esPar(n: integer): boolean {
  return n % 2 == 0;
}
function lista(): integer[] {
  return [1, 2];
}
print(nombre());
print(esPar(4));
