#include "control_flow_checker.h"

#include "diagnostics/codes.h"

namespace compiscript {
namespace semantic {

using namespace compiscript::ast;

ControlFlowChecker::ControlFlowChecker(diagnostics::DiagnosticReporter& reporter)
    : reporter_(reporter) {}

void ControlFlowChecker::checkDeadCode(const std::vector<StatementPtr>& statements) {
    bool seenTerminator = false;
    bool reported = false;
    for (auto& stmt : statements) {
        if (seenTerminator && !reported) {
            reporter_.error(diagnostics::codes::SEM012,
                             "codigo inalcanzable: nunca se llega a ejecutar esta instruccion.",
                             stmt->line, stmt->column);
            reported = true;
        }
        Statement* s = stmt.get();
        if (dynamic_cast<ReturnStatement*>(s) || dynamic_cast<BreakStatement*>(s) ||
            dynamic_cast<ContinueStatement*>(s)) {
            seenTerminator = true;
        }
    }
}

void ControlFlowChecker::run(Program& program) {
    checkDeadCode(program.statements);
    for (auto& stmt : program.statements) checkStatement(stmt.get(), false, false);
}

void ControlFlowChecker::checkStatement(Statement* stmt, bool insideLoop, bool insideFunction) {
    if (auto* n = dynamic_cast<BreakStatement*>(stmt)) {
        if (!insideLoop) {
            reporter_.error(diagnostics::codes::SEM006,
                             "'break' solo puede usarse dentro de un bucle.", n->line, n->column);
        }
        return;
    }
    if (auto* n = dynamic_cast<ContinueStatement*>(stmt)) {
        if (!insideLoop) {
            reporter_.error(diagnostics::codes::SEM006,
                             "'continue' solo puede usarse dentro de un bucle.", n->line,
                             n->column);
        }
        return;
    }
    if (auto* n = dynamic_cast<ReturnStatement*>(stmt)) {
        if (!insideFunction) {
            reporter_.error(diagnostics::codes::SEM007,
                             "'return' solo puede usarse dentro de una funcion.", n->line,
                             n->column);
        }
        return;
    }
    if (auto* n = dynamic_cast<Block*>(stmt)) {
        checkDeadCode(n->statements);
        for (auto& s : n->statements) checkStatement(s.get(), insideLoop, insideFunction);
        return;
    }
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) {
        checkStatement(n->then_block.get(), insideLoop, insideFunction);
        if (n->else_block) checkStatement(n->else_block.get(), insideLoop, insideFunction);
        return;
    }
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) {
        checkStatement(n->body.get(), true, insideFunction);
        return;
    }
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) {
        checkStatement(n->body.get(), true, insideFunction);
        return;
    }
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) {
        checkStatement(n->body.get(), true, insideFunction);
        return;
    }
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) {
        checkStatement(n->body.get(), true, insideFunction);
        return;
    }
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        // Ver comentario en el .h: switch NO cuenta como bucle, se
        // mantiene `insideLoop` tal como venia (si ya habia un bucle
        // envolvente real, break/continue siguen siendo validos; si no,
        // siguen sin serlo).
        for (auto& c : n->cases) {
            checkDeadCode(c->statements);
            for (auto& s : c->statements) checkStatement(s.get(), insideLoop, insideFunction);
        }
        checkDeadCode(n->default_statements);
        for (auto& s : n->default_statements) checkStatement(s.get(), insideLoop, insideFunction);
        return;
    }
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) {
        checkStatement(n->try_block.get(), insideLoop, insideFunction);
        checkStatement(n->catch_block.get(), insideLoop, insideFunction);
        return;
    }
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) {
        // Una funcion (incluso anidada dentro de un bucle) arranca su
        // propio contexto: un 'break' de su cuerpo NO se refiere al bucle
        // de afuera.
        checkDeadCode(n->body->statements);
        for (auto& s : n->body->statements) checkStatement(s.get(), false, true);
        return;
    }
    if (auto* n = dynamic_cast<ClassDeclaration*>(stmt)) {
        for (auto& member : n->members) checkStatement(member.get(), false, false);
        return;
    }

    // Los demas statements (declaraciones, asignaciones, expresiones) no
    // contienen sub-statements ni son break/continue/return.
}

}  // namespace semantic
}  // namespace compiscript
