#include "declaration_collector.h"

#include "diagnostics/codes.h"

namespace compiscript {
namespace semantic {

using namespace compiscript::ast;

DeclarationCollector::DeclarationCollector(SymbolTable& table,
                                            diagnostics::DiagnosticReporter& reporter)
    : table_(table), reporter_(reporter) {}

void DeclarationCollector::declareOrReport(Scope* scope, SymbolPtr symbol) {
    auto conflict = scope->declare(symbol);
    if (conflict != nullptr) {
        reporter_.error(diagnostics::codes::SEM002,
                         "'" + symbol->name + "' ya fue declarado en la linea " +
                             std::to_string(conflict->declared_line) + ".",
                         symbol->declared_line, symbol->declared_column);
    }
}

void DeclarationCollector::run(Program& program) {
    Scope* global = table_.global();
    program.scope = global;
    collectBlockBody(program.statements, global);
}

void DeclarationCollector::collectBlockBody(const std::vector<StatementPtr>& statements,
                                             Scope* scope) {
    for (auto& stmt : statements) {
        collectStatement(stmt.get(), scope);
    }
}

void DeclarationCollector::collectBlock(Block* block, Scope* parentScope) {
    Scope* blockScope = parentScope->createChild(ScopeKind::Block);
    block->scope = blockScope;
    collectBlockBody(block->statements, blockScope);
}

void DeclarationCollector::collectStatement(Statement* stmt, Scope* scope) {
    stmt->scope = scope;

    if (auto* n = dynamic_cast<VariableDeclaration*>(stmt)) {
        auto sym = std::make_shared<Symbol>();
        sym->name = n->name;
        sym->kind = SymbolKind::Variable;
        sym->declared_type = n->declared_type;
        sym->declared_line = n->line;
        sym->declared_column = n->column;
        sym->is_mutable = true;
        sym->declaration = n;
        declareOrReport(scope, sym);
        n->symbol = sym.get();
        return;
    }
    if (auto* n = dynamic_cast<ConstantDeclaration*>(stmt)) {
        auto sym = std::make_shared<Symbol>();
        sym->name = n->name;
        sym->kind = SymbolKind::Constant;
        sym->declared_type = n->declared_type;
        sym->declared_line = n->line;
        sym->declared_column = n->column;
        sym->is_mutable = false;
        sym->declaration = n;
        declareOrReport(scope, sym);
        n->symbol = sym.get();
        return;
    }
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) {
        auto fn = std::make_shared<FunctionSymbol>();
        fn->name = n->name;
        fn->kind = SymbolKind::Function;
        fn->params = n->params;
        fn->return_type = n->return_type;
        fn->declared_line = n->line;
        fn->declared_column = n->column;
        declareOrReport(scope, fn);
        n->symbol = fn.get();

        Scope* funcScope = scope->createChild(ScopeKind::Function);
        fn->function_scope = funcScope;
        funcScope->owner = fn.get();
        n->scope = scope;  // scope donde el NOMBRE de la funcion es visible

        for (auto& param : n->params) {
            auto paramSym = std::make_shared<Symbol>();
            paramSym->name = param.name;
            paramSym->kind = SymbolKind::Parameter;
            paramSym->declared_type = param.declared_type;
            paramSym->declared_line = param.line;
            paramSym->declared_column = param.column;
            declareOrReport(funcScope, paramSym);
        }

        // El cuerpo comparte funcScope directamente (no se anida un Block
        // scope adicional para el cuerpo inmediato de la funcion): mismo
        // criterio que el diagrama de los apuntes de clase, donde
        // FunctionScope contiene los parametros y los locales del cuerpo
        // al mismo nivel.
        n->body->scope = funcScope;
        collectBlockBody(n->body->statements, funcScope);
        return;
    }
    if (auto* n = dynamic_cast<ClassDeclaration*>(stmt)) {
        auto cls = std::make_shared<ClassSymbol>();
        cls->name = n->name;
        cls->kind = SymbolKind::Class;
        cls->base_class_name = n->base_name;
        cls->declared_line = n->line;
        cls->declared_column = n->column;
        declareOrReport(scope, cls);
        n->symbol = cls.get();
        n->scope = scope;

        Scope* classScope = scope->createChild(ScopeKind::Class);
        cls->class_scope = classScope;
        classScope->owner = cls.get();
        for (auto& member : n->members) {
            collectStatement(member.get(), classScope);
        }
        return;
    }
    if (auto* n = dynamic_cast<Block*>(stmt)) {
        collectBlock(n, scope);
        return;
    }
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) {
        collectBlock(n->then_block.get(), scope);
        if (n->else_block) collectBlock(n->else_block.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) {
        collectBlock(n->body.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) {
        collectBlock(n->body.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) {
        // La variable de 'init' (si es 'let i = ...') queda en un scope
        // propio del for, para que no se filtre fuera del loop.
        Scope* loopScope = scope->createChild(ScopeKind::Block);
        n->scope = loopScope;
        if (n->init) {
            collectStatement(n->init.get(), loopScope);
        }
        collectBlock(n->body.get(), loopScope);
        return;
    }
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) {
        Scope* loopScope = scope->createChild(ScopeKind::Block);
        n->scope = loopScope;
        auto sym = std::make_shared<Symbol>();
        sym->name = n->var_name;
        sym->kind = SymbolKind::Variable;
        sym->declared_type = nullptr;  // 'foreach (x in ...)' no anota tipo
        sym->declared_line = n->line;
        sym->declared_column = n->column;
        declareOrReport(loopScope, sym);
        collectBlock(n->body.get(), loopScope);
        return;
    }
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        // La gramatica no pone '{ }' alrededor de cada case: no hay limite
        // sintactico para darle scope propio, asi que todos los case y el
        // default comparten el scope del switch.
        for (auto& c : n->cases) {
            c->scope = scope;
            collectBlockBody(c->statements, scope);
        }
        if (n->has_default) {
            collectBlockBody(n->default_statements, scope);
        }
        return;
    }
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) {
        collectBlock(n->try_block.get(), scope);

        Scope* catchScope = scope->createChild(ScopeKind::Block);
        auto errSym = std::make_shared<Symbol>();
        errSym->name = n->error_name;
        errSym->kind = SymbolKind::Variable;
        errSym->declared_type = std::make_shared<NamedTypeAnnotation>("string");
        errSym->declared_line = n->line;
        errSym->declared_column = n->column;
        declareOrReport(catchScope, errSym);
        n->catch_block->scope = catchScope;
        collectBlockBody(n->catch_block->statements, catchScope);
        return;
    }

    // AssignmentStatement, PropertyAssignment, ExpressionStatement,
    // PrintStatement, ReturnStatement, BreakStatement, ContinueStatement:
    // no declaran nada y no contienen statements anidados. Ya se les fijo
    // `scope` al entrar a esta funcion; no hay nada mas que hacer aqui
    // (resolver las expresiones que contienen es trabajo del proximo pass,
    // NameResolver).
}

}  // namespace semantic
}  // namespace compiscript
