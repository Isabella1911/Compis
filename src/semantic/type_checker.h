#ifndef COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H
#define COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H

// Pass 3 del analisis semantico: recorre el AST llenando
// AstNode::resolved_type (de abajo hacia arriba: el tipo de un nodo se
// calcula a partir de sus hijos, como explican los apuntes de clase sobre
// atributos sintetizados) y valida las reglas de "Sistema de Tipos" y
// parte de "Control de Flujo" del PDF.
//
// Cubre: tipos en operaciones aritmeticas/logicas/comparaciones (SEM004),
// tipos en asignaciones (SEM003), condiciones booleanas de
// if/while/do-while/for/ternario (SEM005), tipo de retorno (SEM009),
// compatibilidad de tipos en switch/case (SEM004), tipos de elementos de
// arreglo e indices (SEM004), y nombres de tipo invalidos en anotaciones
// (SEM013).
//
// Deliberadamente NO cubre todavia (ver docs/03_passes_semanticos.md):
//   - resolver member_name en accesos a atributos/metodos (SEM010):
//     necesita la cadena de herencia resuelta primero.
//   - validar numero/tipo de argumentos en llamadas (SEM008): idem, para
//     llamadas a metodos.
//   - tipar 'this' correctamente.
//   - break/continue fuera de bucle (SEM006) y code muerto (SEM012): son
//     control de flujo puro, no necesitan tipos -- pass aparte.

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
    void checkStatement(ast::Statement* stmt);
    TypePtr checkExpression(ast::Expression* expr, Scope* scope);

    // Recorre una anotacion de tipo (declarada, o null) y la resuelve.
    TypePtr resolveDeclaredType(const ast::TypeAnnotationPtr& annotation, Scope* scope);

    // Chequea que `actual` sea compatible con `expected`; si no, reporta
    // `code` en (line, column). Retorna si eran compatibles.
    bool checkCompatible(const TypePtr& expected, const TypePtr& actual, const char* code,
                          const std::string& message, int line, int column);

    Scope* findEnclosingFunctionScope(Scope* scope) const;

    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H
