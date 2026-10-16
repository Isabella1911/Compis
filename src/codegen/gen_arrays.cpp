// Parte 2: listas, acceso por indice y foreach.
// Ver docs/reparto_tac/parte2_funciones_clases_listas.md, seccion 2.4.

#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;

Operand TacGenerator::genArrayLiteral(ArrayLiteral* /*n*/) {
    return todo("parte 2", "literal de lista");
}

Operand TacGenerator::genArrayAccess(ArrayAccessExpression* /*n*/) {
    return todo("parte 2", "acceso por indice");
}

Operand TacGenerator::genIndexAssignment(ArrayAccessExpression* /*target*/,
                                         Expression* /*value*/) {
    return todo("parte 2", "asignacion por indice");
}

void TacGenerator::genForeach(ForeachStatement* /*n*/) { todo("parte 2", "foreach"); }

}  // namespace codegen
}  // namespace compiscript
