// Prueba estrella A: una clase completa sola, sin instancias ni codigo
// ejecutable (dos atributos y uno con inicializador, constructor, un
// metodo que lee el estado y otro que lo modifica). Tiene que compilar a un
// .asm que MARS ensambla y ejecuta: main solo termina, sin salida, codigo 0.
// expect-out:
class CuentaBancaria {
  let titular: string;
  let saldo: integer;
  let movimientos: integer = 0;
  function constructor(titular: string, saldoInicial: integer) {
    this.titular = titular;
    this.saldo = saldoInicial;
  }
  function consultar(): integer {
    return this.saldo;
  }
  function depositar(monto: integer): integer {
    this.saldo = this.saldo + monto;
    this.movimientos = this.movimientos + 1;
    return this.saldo;
  }
}
