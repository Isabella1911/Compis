// Contraparte valida de SEM001: todo identificador usado esta declarado,
// incluso si se declara en un ambito exterior.
let x: integer = 5;
function usar(): integer {
  return x + 1;
}
print(usar());
