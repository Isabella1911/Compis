// Literales en la tabla de strings (los repetidos comparten etiqueta) y
// operaciones de contenido delegadas al runtime (__concat, __str_eq).
let nombre: string = "Compi";
let saludo: string = "hola " + nombre;
let igual: boolean = saludo == "hola Compi";
let distinto: boolean = nombre != "Compi";
print(saludo);
print(igual);
print(distinto);
print("hola ");
print(null);
