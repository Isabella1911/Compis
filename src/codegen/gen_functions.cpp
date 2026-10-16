// Parte 2: funciones, llamadas, return y closures.
// Ver el documento de reparto de la parte 2, seccion 2.1 y 2.2.

#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;

void TacGenerator::genFunctionDeclaration(FunctionDeclaration* n) {
    todo("parte 2", "funcion '" + n->name + "'");
}

void TacGenerator::genReturn(ReturnStatement* /*n*/) { todo("parte 2", "return"); }

Operand TacGenerator::genCall(CallExpression* /*n*/) { return todo("parte 2", "llamada"); }

}  // namespace codegen
}  // namespace compiscript
