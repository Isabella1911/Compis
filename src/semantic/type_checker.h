#ifndef COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H
#define COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H

// Prepara tipos declarados y firmas antes de revisar cuerpos y expresiones.
// ControlFlowChecker y ClosureAnalyzer conservan sus recorridos independientes.
// Decisiones y diagnosticos: docs/03_passes_semanticos.md.

#include <unordered_set>

#include "ast/nodes.h"
#include "diagnostics/reporter.h"
#include "scope.h"
#include "type.h"

namespace compiscript {
namespace semantic {

class TypeChecker {
public:
    explicit TypeChecker(diagnostics::DiagnosticReporter& reporter);

    void run(ast::Program& program);

private:
    void prepareScope(Scope* scope);
    void prepareSymbol(Symbol* symbol);
    TypePtr symbolType(Symbol* symbol, int line, int column);
    bool checkWritable(Symbol* symbol, int line, int column);
    bool checkLvalue(ast::Expression* expression);
    std::unordered_set<ast::Statement*> checked_;

    void checkStatement(ast::Statement* stmt);
    TypePtr checkExpression(ast::Expression* expr, Scope* scope);

    // Recorre una anotacion de tipo (declarada, o null) y la resuelve.
    TypePtr resolveDeclaredType(const ast::TypeAnnotationPtr& annotation, Scope* scope);

    // Chequea que `actual` sea compatible con `expected`; si no, reporta
    // `code` en (line, column). Retorna si eran compatibles.
    bool checkCompatible(const TypePtr& expected, const TypePtr& actual, const char* code,
                          const std::string& message, int line, int column);

    Scope* findEnclosingFunctionScope(Scope* scope) const;
    Scope* findEnclosingClassScope(Scope* scope) const;

    // Busca `name` en `cls` y, si no esta, sube por su cadena de
    // herencia (base_class). No propietario (vive en algun class_scope).
    Symbol* lookupMember(ClassSymbol* cls, const std::string& name) const;

    // Resuelve el acceso `objectExpr.memberName`: chequea el objeto,
    // valida que sea una clase, busca el miembro (subiendo por la
    // herencia). Retorna nullptr si ya se reporto un error (o si el
    // objeto arrastra un error anterior, para no reportar en cascada).
    Symbol* resolveMemberAccess(ast::Expression* objectExpr, const std::string& memberName,
                                 Scope* scope, int line, int column);

    // Compara argTypes contra paramTypes (numero y tipo, posicional);
    // reporta SEM008 si no coinciden.
    void checkArguments(const std::vector<TypePtr>& paramTypes,
                         const std::vector<TypePtr>& argTypes, int line, int column);

    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H
