#ifndef COMPISCRIPT_SEMANTIC_CONTROL_FLOW_CHECKER_H
#define COMPISCRIPT_SEMANTIC_CONTROL_FLOW_CHECKER_H

// Valida contextos de break/continue/return, retornos completos y codigo muerto.
// El resumen estructural combina caminos sin construir un CFG ni ejecutar codigo.
// switch no habilita break/continue: solo los bucles introducen ese contexto.

#include "ast/nodes.h"
#include "diagnostics/reporter.h"

namespace compiscript {
namespace semantic {

class ControlFlowChecker {
public:
    explicit ControlFlowChecker(diagnostics::DiagnosticReporter& reporter);

    void run(ast::Program& program);

private:
    enum Flow : unsigned { FallsThrough = 1, Returns = 2, Breaks = 4, Continues = 8 };
    unsigned flowOf(ast::Statement* stmt) const;
    unsigned flowOf(const std::vector<ast::StatementPtr>& statements) const;
    unsigned loopFlow(ast::Block* body, ast::Expression* condition,
                      bool runsOnce, bool omittedMeansTrue = false) const;

    void checkStatement(ast::Statement* stmt, bool insideLoop, bool insideFunction);

    // Reporta SEM012 en el primer statement inalcanzable de la lista (uno
    // solo por lista, no uno por cada statement que sigue).
    void checkDeadCode(const std::vector<ast::StatementPtr>& statements);

    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_CONTROL_FLOW_CHECKER_H
