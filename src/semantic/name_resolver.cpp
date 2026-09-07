#include "name_resolver.h"

#include "diagnostics/codes.h"

namespace compiscript {
namespace semantic {

using namespace compiscript::ast;

NameResolver::NameResolver(diagnostics::DiagnosticReporter& reporter) : reporter_(reporter) {}

SymbolPtr NameResolver::resolveOrReport(Scope* scope, const std::string& name, int line,
                                         int column) {
    auto symbol = scope->resolve(name);
    if (symbol == nullptr) {
        reporter_.error(diagnostics::codes::SEM001, "'" + name + "' no esta declarado.", line,
                         column);
    }
    return symbol;
}

void NameResolver::run(Program& program) {
    for (auto& stmt : program.statements) {
        resolveStatement(stmt.get());
    }
}

void NameResolver::resolveStatement(Statement* stmt) {
    // DeclarationCollector ya dejo `scope` correcto en TODO statement: no
    // hace falta que este pass vuelva a rastrearlo.
    Scope* scope = stmt->scope;

    if (auto* n = dynamic_cast<VariableDeclaration*>(stmt)) {
        if (n->initializer) resolveExpression(n->initializer.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<ConstantDeclaration*>(stmt)) {
        resolveExpression(n->initializer.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<AssignmentStatement*>(stmt)) {
        n->symbol = resolveOrReport(scope, n->target_name, n->line, n->column).get();
        resolveExpression(n->value.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<PropertyAssignment*>(stmt)) {
        resolveExpression(n->object.get(), scope);
        // n->member_name se deja sin resolver: requiere el tipo de object.
        resolveExpression(n->value.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<ExpressionStatement*>(stmt)) {
        resolveExpression(n->expression.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<PrintStatement*>(stmt)) {
        resolveExpression(n->expression.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<Block*>(stmt)) {
        for (auto& s : n->statements) resolveStatement(s.get());
        return;
    }
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) {
        resolveExpression(n->condition.get(), scope);
        resolveStatement(n->then_block.get());
        if (n->else_block) resolveStatement(n->else_block.get());
        return;
    }
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) {
        resolveExpression(n->condition.get(), scope);
        resolveStatement(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) {
        resolveStatement(n->body.get());
        resolveExpression(n->condition.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) {
        // El scope de 'init' es el propio ForStatement (n->scope), que
        // DeclarationCollector ya uso para declarar la variable de init:
        // reusarlo aqui para condition/update es correcto.
        if (n->init) resolveStatement(n->init.get());
        if (n->condition) resolveExpression(n->condition.get(), scope);
        if (n->update) resolveExpression(n->update.get(), scope);
        resolveStatement(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) {
        resolveExpression(n->iterable.get(), scope);
        resolveStatement(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        resolveExpression(n->subject.get(), scope);
        for (auto& c : n->cases) {
            resolveExpression(c->expression.get(), scope);
            for (auto& s : c->statements) resolveStatement(s.get());
        }
        for (auto& s : n->default_statements) resolveStatement(s.get());
        return;
    }
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) {
        resolveStatement(n->try_block.get());
        resolveStatement(n->catch_block.get());
        return;
    }
    if (auto* n = dynamic_cast<ReturnStatement*>(stmt)) {
        if (n->value) resolveExpression(n->value.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) {
        // El cuerpo tiene su propio scope (funcScope), ya guardado en
        // n->body->scope por DeclarationCollector.
        for (auto& s : n->body->statements) resolveStatement(s.get());
        return;
    }
    if (auto* n = dynamic_cast<ClassDeclaration*>(stmt)) {
        for (auto& member : n->members) resolveStatement(member.get());
        return;
    }

    // BreakStatement, ContinueStatement: no contienen expresiones.
}

void NameResolver::resolveExpression(Expression* expr, Scope* scope) {
    if (expr == nullptr) return;
    expr->scope = scope;

    if (auto* n = dynamic_cast<AssignmentExpression*>(expr)) {
        resolveExpression(n->target.get(), scope);
        resolveExpression(n->value.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<PropertyAssignExpr*>(expr)) {
        resolveExpression(n->object.get(), scope);
        // n->member_name se deja sin resolver, igual que en PropertyAssignment.
        resolveExpression(n->value.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<TernaryExpression*>(expr)) {
        resolveExpression(n->condition.get(), scope);
        resolveExpression(n->then_expr.get(), scope);
        resolveExpression(n->else_expr.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<BinaryExpression*>(expr)) {
        resolveExpression(n->left.get(), scope);
        resolveExpression(n->right.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<UnaryExpression*>(expr)) {
        resolveExpression(n->operand.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<ArrayLiteral*>(expr)) {
        for (auto& e : n->elements) resolveExpression(e.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<IdentifierExpression*>(expr)) {
        n->symbol = resolveOrReport(scope, n->name, n->line, n->column).get();
        return;
    }
    if (auto* n = dynamic_cast<NewExpression*>(expr)) {
        n->symbol = resolveOrReport(scope, n->class_name, n->line, n->column).get();
        for (auto& a : n->arguments) resolveExpression(a.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<CallExpression*>(expr)) {
        resolveExpression(n->callee.get(), scope);
        for (auto& a : n->arguments) resolveExpression(a.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<ArrayAccessExpression*>(expr)) {
        resolveExpression(n->array.get(), scope);
        resolveExpression(n->index.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<MemberAccessExpression*>(expr)) {
        resolveExpression(n->object.get(), scope);
        // n->member_name se deja sin resolver: requiere el tipo de object.
        return;
    }

    // LiteralExpression, ThisExpression: no requieren busqueda de nombre.
}

}  // namespace semantic
}  // namespace compiscript
