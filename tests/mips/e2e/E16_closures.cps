// E16: funciones anidadas que leen y MODIFICAN variables de la funcion que
// las contiene (contador sobrevive entre llamadas), una anidada que llama
// a su hermana, y captura a dos niveles de profundidad.
// expect-out:
// 110
// 113
// 123
function exterior(base: integer): integer {
  let contador: integer = 0;
  function incrementa(paso: integer): integer {
    contador = contador + paso;
    return contador + base;
  }
  function dosVeces(): integer {
    incrementa(1);
    return incrementa(2);
  }
  print(incrementa(10));
  return dosVeces();
}
function nivel1(a: integer): integer {
  function nivel2(b: integer): integer {
    function nivel3(c: integer): integer {
      return a * 100 + b * 10 + c;
    }
    return nivel3(3);
  }
  return nivel2(2);
}
print(exterior(100));
print(nivel1(1));
