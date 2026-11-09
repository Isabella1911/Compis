// Prueba estrella B: la misma clase, con DOS objetos distintos. Cada uno
// guarda su propio estado: los depositos de uno no cambian al otro.
// expect-out:
// Ana
// 120
// 1
// Luis
// 60
// 2
// 121
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
let a: CuentaBancaria = new CuentaBancaria("Ana", 100);
let b: CuentaBancaria = new CuentaBancaria("Luis", 50);
a.depositar(20);
b.depositar(5);
b.depositar(5);
print(a.titular);
print(a.consultar());
print(a.movimientos);
print(b.titular);
print(b.consultar());
print(b.movimientos);
print(a.depositar(1));
