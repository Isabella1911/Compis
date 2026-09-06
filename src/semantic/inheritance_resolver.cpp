#include "inheritance_resolver.h"

#include <set>

#include "diagnostics/codes.h"
#include "scope.h"
#include "symbol.h"

namespace compiscript {
namespace semantic {

using namespace compiscript::ast;

InheritanceResolver::InheritanceResolver(diagnostics::DiagnosticReporter& reporter)
    : reporter_(reporter) {}

void InheritanceResolver::collectClasses(Statement* stmt, std::vector<ClassDeclaration*>& out) {
    if (auto* n = dynamic_cast<ClassDeclaration*>(stmt)) {
        out.push_back(n);
        for (auto& member : n->members) collectClasses(member.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<Block*>(stmt)) {
        for (auto& s : n->statements) collectClasses(s.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) {
        collectClasses(n->then_block.get(), out);
        if (n->else_block) collectClasses(n->else_block.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) {
        collectClasses(n->body.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) {
        collectClasses(n->body.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) {
        if (n->init) collectClasses(n->init.get(), out);
        collectClasses(n->body.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) {
        collectClasses(n->body.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        for (auto& c : n->cases)
            for (auto& s : c->statements) collectClasses(s.get(), out);
        for (auto& s : n->default_statements) collectClasses(s.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) {
        collectClasses(n->try_block.get(), out);
        collectClasses(n->catch_block.get(), out);
        return;
    }
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) {
        for (auto& s : n->body->statements) collectClasses(s.get(), out);
        return;
    }
    // Los demas statements no pueden contener una ClassDeclaration.
}

void InheritanceResolver::detectCycle(ClassDeclaration* classDecl) {
    auto* cls = dynamic_cast<ClassSymbol*>(classDecl->symbol);
    if (cls == nullptr) return;

    std::set<ClassSymbol*> seen;
    for (ClassSymbol* current = cls; current != nullptr; current = current->base_class) {
        if (seen.count(current)) {
            reporter_.error(diagnostics::codes::SEM014,
                             "Herencia circular: '" + cls->name +
                                 "' termina heredando de si misma a traves de '" + current->name +
                                 "'.",
                             classDecl->line, classDecl->column);
            return;
        }
        seen.insert(current);
    }
}

void InheritanceResolver::run(Program& program) {
    std::vector<ClassDeclaration*> classes;
    for (auto& stmt : program.statements) collectClasses(stmt.get(), classes);

    // Paso 1: resolver cada base_class_name a un ClassSymbol real. Se hace
    // para TODAS las clases antes de buscar ciclos, porque una clase
    // puede heredar de otra declarada mas abajo en el archivo -- para
    // cuando este paso corre, DeclarationCollector ya declaro todas, asi
    // que el orden de aparicion en el AST no importa.
    for (auto* classDecl : classes) {
        auto* cls = dynamic_cast<ClassSymbol*>(classDecl->symbol);
        if (cls == nullptr || !cls->base_class_name) continue;

        auto base = classDecl->scope->resolve(*cls->base_class_name);
        auto baseClass = std::dynamic_pointer_cast<ClassSymbol>(base);
        if (baseClass == nullptr) {
            reporter_.error(diagnostics::codes::SEM013,
                             "'" + *cls->base_class_name + "' no es una clase valida para heredar.",
                             classDecl->line, classDecl->column);
            continue;
        }
        cls->base_class = baseClass.get();
    }

    // Paso 2: con todos los base_class ya enlazados, detectar ciclos.
    for (auto* classDecl : classes) {
        detectCycle(classDecl);
    }
}

}  // namespace semantic
}  // namespace compiscript
