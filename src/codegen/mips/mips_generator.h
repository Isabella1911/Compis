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
//   - param se emiten RTL en el TAC; el llamador los apila (push) en ese orden
//   - link (static link) se apila antes del call si aplica
//   - retorno en $v0; $ra/$fp preservados por el llamado
//   - Builtins del runtime: __concat, __str_eq, __runtime_error
//
// Opcodes no implementados en esta fase producen CG001/CG004 y no hay .asm.

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
