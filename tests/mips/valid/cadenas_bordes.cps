// Cadenas en el borde: la vacia, concatenar con ella, compararla, barra
// invertida y comilla simple. Compiscript no tiene secuencias de escape: la
// barra es un caracter mas y el backend la escapa al escribir el .asciiz.
// expect-out:
// []
// true
// false
// [a\b]
// it's
// abcabc
let vacia: string = "";
print("[" + vacia + "]");
print(vacia == "");
print(vacia == " ");
print("[a\b]");
print("it's");
let s: string = "abc";
print(s + vacia + s);
