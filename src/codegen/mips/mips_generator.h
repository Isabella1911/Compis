#ifndef COMPISCRIPT_CODEGEN_MIPS_GENERATOR_H
#define COMPISCRIPT_CODEGEN_MIPS_GENERATOR_H

// Traduce ir::TacProgram a ensamblador MIPS32 compatible con MARS.
//
// Politica de ubicaciones (Fase 3 / rúbrica: uso de pila):
//   - Globales: etiquetas .word en .data
//   - Temporales: palabras en el frame, offset = -(frame_size + 4*(t+1))($fp)
//   - Locales/params: offsets de FrameLayout relativos a $fp
//   - $t0/$t1/$t2 solo como scratch al bajar una instruccion; no se asignan
//     temporales a registros de forma persistente (evita spills ante calls).
//
// Convención de llamada (derivada de docs/08):
//   - param se emiten RTL en el TAC; cada uno se apila al aparecer
//   - link (static link) se apila justo antes del call; 0 si no hay
//   - call directa con jal, indirecta (vtable) con jalr
//   - retorno en $v0; $ra/$fp preservados por el llamado; el llamador
//     limpia argumentos y enlace. main verifica al final que $sp quedo
//     balanceado (ver tests/mips/opcode_support.md)
//   - Builtins del runtime: __concat, __str_eq, __raise y los errores
//     __div_zero, __null_ref, __runtime_error (todos terminan en __fail)
//
// Heap: alloc usa syscall 9 (sbrk) con tamano alineado a 4. Load/Store
// revisan que la base no sea null (salvo fp, en closures).
//
// try/catch: pila de handlers en el runtime; todo error de runtime pasa por
// __fail, que salta al catch activo o termina con codigo 1.
//
// Un opcode o caso no soportado produce CG001/CG004 y no hay .asm.

#include "ir/tac.h"
#include "mips_result.h"

namespace compiscript {
namespace codegen {
namespace mips {

MipsResult generateMips(const ir::TacProgram& program);

}  // namespace mips
}  // namespace codegen
}  // namespace compiscript

#endif  // COMPISCRIPT_CODEGEN_MIPS_GENERATOR_H
