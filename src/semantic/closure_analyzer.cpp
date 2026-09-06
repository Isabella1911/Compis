#include "closure_analyzer.h"

#include <algorithm>

namespace compiscript {
namespace semantic {

using namespace compiscript::ast;

bool ClosureAnalyzer::isCaptured(Scope* useSiteScope, Scope* funcScope,
                                  const std::string& name) const {
    bool pastFuncScope = false;
    for (Scope* s = useSiteScope; s != nullptr; s = s->parent()) {
        if (s->resolveLocal(name)) {
            return pastFuncScope;
        }
        if (s == funcScope) pastFuncScope = true;
    }
    return false;  // no deberia pasar: NameResolver ya la resolvio antes
}

void ClosureAnalyzer::run(Program& program) {
    for (auto& stmt : program.statements) findFunctions(stmt.get());
}

void ClosureAnalyzer::findFunctions(Statement* stmt) {
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) {
        analyzeFunctionBody(n);
        return;
    }
    if (auto* n = dynamic_cast<ClassDeclaration*>(stmt)) {
        for (auto& member : n->members) findFunctions(member.get());
        return;
    }
    if (auto* n = dynamic_cast<Block*>(stmt)) {
        for (auto& s : n->statements) findFunctions(s.get());
        return;
    }
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) {
        findFunctions(n->then_block.get());
        if (n->else_block) findFunctions(n->else_block.get());
        return;
    }
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) {
        findFunctions(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) {
        findFunctions(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) {
        if (n->init) findFunctions(n->init.get());
        findFunctions(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) {
        findFunctions(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        for (auto& c : n->cases)
            for (auto& s : c->statements) findFunctions(s.get());
        for (auto& s : n->default_statements) findFunctions(s.get());
        return;
    }
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) {
        findFunctions(n->try_block.get());
        findFunctions(n->catch_block.get());
        return;
    }
    // Las demas no contienen sub-statements ni declaran funciones.
}

void ClosureAnalyzer::analyzeFunctionBody(FunctionDeclaration* fnDecl) {
    auto* fn = dynamic_cast<FunctionSymbol*>(fnDecl->symbol);
    if (fn == nullptr) return;
    Scope* funcScope = fnDecl->body->scope;
    for (auto& stmt : fnDecl->body->statements) walkStatement(stmt.get(), funcScope, fn);
    // Una funcion anidada declarada dentro de este cuerpo ya se analiza
    // aparte (walkStatement la detecta y llama analyzeFunctionBody de
    // nuevo), asi que no hace falta buscarla otra vez desde aca.
}

void ClosureAnalyzer::walkStatement(Statement* stmt, Scope* funcScope, FunctionSymbol* fn) {
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) {
        analyzeFunctionBody(n);  // contexto propio, independiente de 'fn'
        return;
    }
    if (auto* n = dynamic_cast<VariableDeclaration*>(stmt)) {
        walkExpression(n->initializer.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<ConstantDeclaration*>(stmt)) {
        walkExpression(n->initializer.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<AssignmentStatement*>(stmt)) {
        if (n->symbol != nullptr && isCaptured(n->scope, funcScope, n->target_name)) {
            if (std::find(fn->captured.begin(), fn->captured.end(), n->symbol) ==
                fn->captured.end()) {
                fn->captured.push_back(n->symbol);
            }
        }
        walkExpression(n->value.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<PropertyAssignment*>(stmt)) {
        walkExpression(n->object.get(), funcScope, fn);
        walkExpression(n->value.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<ExpressionStatement*>(stmt)) {
        walkExpression(n->expression.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<PrintStatement*>(stmt)) {
        walkExpression(n->expression.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<Block*>(stmt)) {
        for (auto& s : n->statements) walkStatement(s.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) {
        walkExpression(n->condition.get(), funcScope, fn);
        walkStatement(n->then_block.get(), funcScope, fn);
        if (n->else_block) walkStatement(n->else_block.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) {
        walkExpression(n->condition.get(), funcScope, fn);
        walkStatement(n->body.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) {
        walkStatement(n->body.get(), funcScope, fn);
        walkExpression(n->condition.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) {
        if (n->init) walkStatement(n->init.get(), funcScope, fn);
        if (n->condition) walkExpression(n->condition.get(), funcScope, fn);
        if (n->update) walkExpression(n->update.get(), funcScope, fn);
        walkStatement(n->body.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) {
        walkExpression(n->iterable.get(), funcScope, fn);
        walkStatement(n->body.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        walkExpression(n->subject.get(), funcScope, fn);
        for (auto& c : n->cases) {
            walkExpression(c->expression.get(), funcScope, fn);
            for (auto& s : c->statements) walkStatement(s.get(), funcScope, fn);
        }
        for (auto& s : n->default_statements) walkStatement(s.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) {
        walkStatement(n->try_block.get(), funcScope, fn);
        walkStatement(n->catch_block.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<ReturnStatement*>(stmt)) {
        if (n->value) walkExpression(n->value.get(), funcScope, fn);
        return;
    }
    // BreakStatement, ContinueStatement, ClassDeclaration (una clase
    // anidada no es una funcion, no participa de esta captura): nada que
    // recorrer aca.
}

void ClosureAnalyzer::walkExpression(Expression* expr, Scope* funcScope, FunctionSymbol* fn) {
    if (expr == nullptr) return;

    if (auto* n = dynamic_cast<IdentifierExpression*>(expr)) {
        if (n->symbol != nullptr && isCaptured(n->scope, funcScope, n->name)) {
            if (std::find(fn->captured.begin(), fn->captured.end(), n->symbol) ==
                fn->captured.end()) {
                fn->captured.push_back(n->symbol);
            }
        }
        return;
    }
    if (auto* n = dynamic_cast<AssignmentExpression*>(expr)) {
        walkExpression(n->target.get(), funcScope, fn);
        walkExpression(n->value.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<PropertyAssignExpr*>(expr)) {
        walkExpression(n->object.get(), funcScope, fn);
        walkExpression(n->value.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<TernaryExpression*>(expr)) {
        walkExpression(n->condition.get(), funcScope, fn);
        walkExpression(n->then_expr.get(), funcScope, fn);
        walkExpression(n->else_expr.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<BinaryExpression*>(expr)) {
        walkExpression(n->left.get(), funcScope, fn);
        walkExpression(n->right.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<UnaryExpression*>(expr)) {
        walkExpression(n->operand.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<ArrayLiteral*>(expr)) {
        for (auto& e : n->elements) walkExpression(e.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<NewExpression*>(expr)) {
        for (auto& a : n->arguments) walkExpression(a.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<CallExpression*>(expr)) {
        walkExpression(n->callee.get(), funcScope, fn);
        for (auto& a : n->arguments) walkExpression(a.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<ArrayAccessExpression*>(expr)) {
        walkExpression(n->array.get(), funcScope, fn);
        walkExpression(n->index.get(), funcScope, fn);
        return;
    }
    if (auto* n = dynamic_cast<MemberAccessExpression*>(expr)) {
        walkExpression(n->object.get(), funcScope, fn);
        return;
    }
    // LiteralExpression, ThisExpression: no referencian variables externas.
}

}  // namespace semantic
}  // namespace compiscript
