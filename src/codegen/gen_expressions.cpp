// Parte 1: expresiones y condiciones con saltos.

#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;

Operand TacGenerator::genLiteral(LiteralExpression* /*n*/) {
    return todo("parte 1", "literal");
}

Operand TacGenerator::genIdentifier(IdentifierExpression* /*n*/) {
    return todo("parte 1", "identificador");
}

Operand TacGenerator::genBinary(BinaryExpression* n) {
    return todo("parte 1", "operador binario " + n->op);
}

Operand TacGenerator::genUnary(UnaryExpression* n) {
    return todo("parte 1", "operador unario " + n->op);
}

Operand TacGenerator::genAssignment(AssignmentExpression* /*n*/) {
    return todo("parte 1", "asignacion");
}

Operand TacGenerator::genTernary(TernaryExpression* /*n*/) {
    return todo("parte 1", "ternario");
}

void TacGenerator::genCond(Expression* /*expr*/, const std::string& /*trueLabel*/,
                           const std::string& /*falseLabel*/) {
    todo("parte 1", "condicion");
}

}  // namespace codegen
}  // namespace compiscript
