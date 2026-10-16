// Parte 1: declaraciones, print y control de flujo.

#include "semantic/scope.h"
#include "semantic/type.h"
#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Instruction;
using ir::OpCode;
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

void TacGenerator::genIf(IfStatement* n) {
    std::string endLabel = newLabel();
    if (!n->else_block) {
        genCond(n->condition.get(), "", endLabel);
        genBlock(n->then_block.get());
        emitLabel(endLabel);
        return;
    }
    std::string elseLabel = newLabel();
    genCond(n->condition.get(), "", elseLabel);
    genBlock(n->then_block.get());
    emitGoto(endLabel);
    emitLabel(elseLabel);
    genBlock(n->else_block.get());
    emitLabel(endLabel);
}

void TacGenerator::genWhile(WhileStatement* n) {
    std::string beginLabel = newLabel();
    std::string endLabel = newLabel();
    emitLabel(beginLabel);
    genCond(n->condition.get(), "", endLabel);
    pushLoop(endLabel, beginLabel);
    genBlock(n->body.get());
    popLoop();
    emitGoto(beginLabel);
    emitLabel(endLabel);
}

void TacGenerator::genDoWhile(DoWhileStatement* n) {
    std::string beginLabel = newLabel();
    std::string condLabel = newLabel();  // destino de continue: se reevalua la condicion
    std::string endLabel = newLabel();
    emitLabel(beginLabel);
    pushLoop(endLabel, condLabel);
    genBlock(n->body.get());
    popLoop();
    emitLabel(condLabel);
    genCond(n->condition.get(), beginLabel, "");
    emitLabel(endLabel);
}

void TacGenerator::genFor(ForStatement* n) {
    std::string beginLabel = newLabel();
    std::string updateLabel = newLabel();  // destino de continue: se ejecuta el update
    std::string endLabel = newLabel();

    if (n->init) genStatement(n->init.get());
    emitLabel(beginLabel);
    if (n->condition) genCond(n->condition.get(), "", endLabel);  // sin condicion: infinito
    pushLoop(endLabel, updateLabel);
    genBlock(n->body.get());
    popLoop();
    emitLabel(updateLabel);
    if (n->update) release(genExpr(n->update.get()));
    emitGoto(beginLabel);
    emitLabel(endLabel);
}

// Sin fallthrough: cada case termina solo al llegar al siguiente. Es la
// unica lectura coherente con el semantico, que reporta `break` dentro de
// un switch como SEM006 (switch no es un bucle). Por lo mismo, un `break`
// dentro de un switch que esta dentro de un bucle sale del BUCLE.
//
//   (sujeto en s)
//   if s == c0 goto Lcase0     <- todas las comparaciones primero
//   if s == c1 goto Lcase1
//   goto Ldefault              (o Lend si no hay default)
// Lcase0: ... goto Lend
// Lcase1: ... goto Lend
// Ldefault: ...
// Lend:
void TacGenerator::genSwitch(SwitchStatement* n) {
    std::string endLabel = newLabel();
    std::vector<std::string> caseLabels;
    for (size_t i = 0; i < n->cases.size(); i++) caseLabels.push_back(newLabel());
    std::string defaultLabel = n->has_default ? newLabel() : endLabel;

    // El sujeto se evalua una sola vez y queda vivo durante las comparaciones.
    Operand subject = genExpr(n->subject.get());
    bool stringSubject = n->subject->resolved_type &&
                         n->subject->resolved_type->kind == TypeKind::String;
    for (size_t i = 0; i < n->cases.size(); i++) {
        Operand value = genExpr(n->cases[i]->expression.get());
        if (stringSubject) {
            emitParam(value);
            emitParam(subject);
            release(value);
            Operand equal = emitCall("__str_eq", 2, true);
            emitIf(equal, caseLabels[i]);
            release(equal);
        } else {
            emitIfRel(subject, "==", value, caseLabels[i]);
            release(value);
        }
    }
    release(subject);
    emitGoto(defaultLabel);

    for (size_t i = 0; i < n->cases.size(); i++) {
        emitLabel(caseLabels[i]);
        genStatements(n->cases[i]->statements);
        emitGoto(endLabel);
    }
    if (n->has_default) {
        emitLabel(defaultLabel);
        genStatements(n->default_statements);
    }
    emitLabel(endLabel);
}

// No hay `throw` en Compiscript: lo que salta al catch son los errores de
// tiempo de ejecucion (p. ej. indice fuera de rango), que el runtime
// levanta con `call __raise, 1` (param: mensaje). try_begin registra el
// handler y try_end lo quita; al saltar al catch, __error_message devuelve
// el mensaje, que se guarda en la variable del catch.
void TacGenerator::genTryCatch(TryCatchStatement* n) {
    std::string catchLabel = newLabel();
    std::string endLabel = newLabel();

    Instruction begin{OpCode::TryBegin};
    begin.label = catchLabel;
    emit(std::move(begin));
    current().try_depth++;
    genBlock(n->try_block.get());
    current().try_depth--;
    emit(Instruction{OpCode::TryEnd});
    emitGoto(endLabel);

    emitLabel(catchLabel);
    semantic::Symbol* errorVar = n->catch_block->scope->resolveLocal(n->error_name).get();
    storeVar(errorVar, emitCall("__error_message", 0, true));
    genBlock(n->catch_block.get());
    emitLabel(endLabel);
}

void TacGenerator::genBreak(BreakStatement* /*n*/) {
    const LoopLabels* loop = currentLoop();
    if (loop == nullptr) {
        todo("parte 1", "break fuera de bucle");  // el semantico ya lo reporta (SEM006)
        return;
    }
    closeTryHandlers(loop->try_depth);
    emitGoto(loop->break_label);
}

void TacGenerator::genContinue(ContinueStatement* /*n*/) {
    const LoopLabels* loop = currentLoop();
    if (loop == nullptr) {
        todo("parte 1", "continue fuera de bucle");
        return;
    }
    closeTryHandlers(loop->try_depth);
    emitGoto(loop->continue_label);
}

}  // namespace codegen
}  // namespace compiscript
