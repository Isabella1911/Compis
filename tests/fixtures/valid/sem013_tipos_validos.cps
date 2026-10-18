// Contraparte valida de SEM013: solo tipos primitivos, listas y clases
// declaradas (aunque se declaren despues de usarse).
let a: integer = 1;
let b: string = "s";
let c: boolean = true;
let d: integer[] = [1];
let e: Punto = new Punto();
class Punto {
  let x: integer;
}
class Punto3D : Punto {
  let z: integer;
}
