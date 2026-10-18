// return dentro de un try: debe emitir try_end antes del return.
function busca(xs: integer[]): integer {
  try {
    if (xs[0] == 1) {
      return 1;
    }
  } catch (e) {
    print(e);
  }
  return 0;
}

let xs: integer[] = [1, 2];
print(busca(xs));
