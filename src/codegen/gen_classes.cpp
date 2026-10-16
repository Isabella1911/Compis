// Parte 2: clases, new, atributos, metodos y this.
// Ver el documento de reparto de la parte 2, seccion 2.3.

#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;

void TacGenerator::genClassDeclaration(ClassDeclaration* n) {
    todo("parte 2", "clase '" + n->name + "'");
}

Operand TacGenerator::genNew(NewExpression* n) {
    return todo("parte 2", "new " + n->class_name);
}

Operand TacGenerator::genMemberAccess(MemberAccessExpression* n) {
    return todo("parte 2", "acceso a '." + n->member_name + "'");
}

Operand TacGenerator::genPropertyAssignment(Expression* /*object*/, semantic::Symbol* /*member*/,
                                            Expression* /*value*/) {
    return todo("parte 2", "asignacion a atributo");
}

Operand TacGenerator::genThis(ThisExpression* /*n*/) { return todo("parte 2", "this"); }

}  // namespace codegen
}  // namespace compiscript
