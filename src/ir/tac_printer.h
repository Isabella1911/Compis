#ifndef COMPISCRIPT_IR_TAC_PRINTER_H
#define COMPISCRIPT_IR_TAC_PRINTER_H

// Formato de texto del TAC: una instruccion por linea, etiquetas en la
// columna 0 y el resto indentado. Es lo que imprime `compiscript --tac` y
// lo que comparan las pruebas de tests/tac/, asi que cualquier cambio aqui
// cambia los archivos esperados. Especificacion: docs/08_lenguaje_intermedio.md.

#include <string>

#include "tac.h"

namespace compiscript {
namespace ir {

std::string printOperand(const Operand& operand);
std::string printInstruction(const Instruction& instruction);
std::string printProgram(const TacProgram& program);

}  // namespace ir
}  // namespace compiscript

#endif  // COMPISCRIPT_IR_TAC_PRINTER_H
