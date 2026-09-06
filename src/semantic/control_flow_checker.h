#ifndef COMPISCRIPT_SEMANTIC_CONTROL_FLOW_CHECKER_H
#define COMPISCRIPT_SEMANTIC_CONTROL_FLOW_CHECKER_H

// Control de flujo puro: no necesita tipos ni tabla de simbolos, solo
// recorre el AST llevando un par de banderas ("estoy dentro de un bucle",
// "estoy dentro de una funcion"). Reporta:
//   - SEM006: 'break'/'continue' fuera de un while/do-while/for/foreach.
//   - SEM007: 'return' fuera de una funcion.
//   - SEM012: codigo inalcanzable (instrucciones despues de un
//     return/break/continue dentro de la misma lista de statements).
//
// Decision de lenguaje (ver README, seccion de decisiones, punto 5): la
// gramatica permite 'break'/'continue' dentro de un 'switch' sin que haya
// un bucle envolvente, pero se sigue la regla LITERAL del enunciado --
// switch NO cuenta como bucle. Si el equipo decide lo contrario, el unico
// lugar que hay que tocar es el caso de SwitchStatement en
// control_flow_checker.cpp (pasar `insideLoop` en vez de mantenerlo).

#include "ast/nodes.h"
#include "diagnostics/reporter.h"

namespace compiscript {
namespace semantic {

class ControlFlowChecker {
public:
    explicit ControlFlowChecker(diagnostics::DiagnosticReporter& reporter);

    void run(ast::Program& program);

private:
    void checkStatement(ast::Statement* stmt, bool insideLoop, bool insideFunction);

    // Reporta SEM012 en el primer statement inalcanzable de la lista (uno
    // solo por lista, no uno por cada statement que sigue).
    void checkDeadCode(const std::vector<ast::StatementPtr>& statements);

    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_CONTROL_FLOW_CHECKER_H
