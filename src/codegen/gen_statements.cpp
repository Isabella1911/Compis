// Parte 1: declaraciones, print y control de flujo.

#include "semantic/type.h"
#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;
using semantic::TypeKind;

void TacGenerator::genBlock(Block* n) { genStatements(n->statements); }

void TacGenerator::genVariableDeclaration(VariableDeclaration* n) {
    // Sin inicializador se asigna 0 (que tambien es false y null), igual
    // para globales que para locales: asi una variable declarada dentro de
    // un bucle arranca limpia en cada vuelta.
    Operand value = n->initializer ? genExpr(n->initializer.get()) : Operand::makeInt(0);
    storeVar(n->symbol, value);
}

void TacGenerator::genConstantDeclaration(ConstantDeclaration* n) {
    // Una constante es una variable que el semantico ya garantiza que no se
    // reasigna (la gramatica obliga el inicializador).
    storeVar(n->symbol, genExpr(n->initializer.get()));
}

void TacGenerator::genAssignmentStatement(AssignmentStatement* n) {
    storeVar(n->symbol, genExpr(n->value.get()));
}

void TacGenerator::genExpressionStatement(ExpressionStatement* n) {
    release(genExpr(n->expression.get()));
}

void TacGenerator::genPrint(PrintStatement* n) {
    Operand value = genExpr(n->expression.get());
    TypeKind kind = n->expression->resolved_type ? n->expression->resolved_type->kind
                                                 : TypeKind::Error;
    switch (kind) {
        case TypeKind::String: emitPrint("str", value); break;
        case TypeKind::Boolean: emitPrint("bool", value); break;
        case TypeKind::Null: emitPrint("str", internString("null")); break;
        // integer, y por convencion tambien listas y objetos: se imprime la
        // direccion (no hay conversion a texto de objetos en Compiscript).
        default: emitPrint("int", value); break;
    }
    release(value);
}

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
