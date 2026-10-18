// expect: SEM006
function foo(): integer {
  if (true) {
    break;
  }
  return 1;
}
