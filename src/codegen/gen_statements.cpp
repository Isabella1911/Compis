// Parte 1: declaraciones, print y control de flujo.

#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;

void TacGenerator::genBlock(Block* n) { genStatements(n->statements); }

void TacGenerator::genVariableDeclaration(VariableDeclaration* n) {
    todo("parte 1", "declaracion de '" + n->name + "'");
}

void TacGenerator::genConstantDeclaration(ConstantDeclaration* n) {
    todo("parte 1", "constante '" + n->name + "'");
}

void TacGenerator::genAssignmentStatement(AssignmentStatement* n) {
    todo("parte 1", "asignacion a '" + n->target_name + "'");
}

void TacGenerator::genExpressionStatement(ExpressionStatement* n) {
    release(genExpr(n->expression.get()));
}

void TacGenerator::genPrint(PrintStatement* /*n*/) { todo("parte 1", "print"); }

void TacGenerator::genIf(IfStatement* /*n*/) { todo("parte 1", "if"); }

void TacGenerator::genWhile(WhileStatement* /*n*/) { todo("parte 1", "while"); }

void TacGenerator::genDoWhile(DoWhileStatement* /*n*/) { todo("parte 1", "do-while"); }

void TacGenerator::genFor(ForStatement* /*n*/) { todo("parte 1", "for"); }

void TacGenerator::genSwitch(SwitchStatement* /*n*/) { todo("parte 1", "switch"); }

void TacGenerator::genTryCatch(TryCatchStatement* /*n*/) { todo("parte 1", "try/catch"); }

void TacGenerator::genBreak(BreakStatement* /*n*/) { todo("parte 1", "break"); }

void TacGenerator::genContinue(ContinueStatement* /*n*/) { todo("parte 1", "continue"); }

}  // namespace codegen
}  // namespace compiscript
