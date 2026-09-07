#include "control_flow_checker.h"

#include "diagnostics/codes.h"
#include "semantic/type.h"

namespace compiscript {
namespace semantic {

using namespace compiscript::ast;

ControlFlowChecker::ControlFlowChecker(diagnostics::DiagnosticReporter& reporter)
    : reporter_(reporter) {}

unsigned ControlFlowChecker::flowOf(const std::vector<StatementPtr>& statements) const {
    unsigned result = FallsThrough;
    for (const auto& statement : statements) {
        if (!(result & FallsThrough)) break;
        result = (result & ~FallsThrough) | flowOf(statement.get());
    }
    return result;
}

unsigned ControlFlowChecker::loopFlow(Block* body, Expression* condition, bool runsOnce,
                                      bool omittedMeansTrue) const {
    unsigned bodyFlow = flowOf(body);
    auto* literal = dynamic_cast<LiteralExpression*>(condition);
    bool always = (!condition && omittedMeansTrue) ||
        (literal && literal->kind == LiteralKind::Boolean && literal->value == "true");
    bool never = literal && literal->kind == LiteralKind::Boolean && literal->value == "false";
    if (never && !runsOnce) return FallsThrough;
    unsigned result = bodyFlow & Returns;
    if ((bodyFlow & Breaks) || (!always && (!runsOnce ||
        (bodyFlow & (FallsThrough | Continues))))) result |= FallsThrough;
    return result;  // break/continue quedan consumidos por este bucle
}

unsigned ControlFlowChecker::flowOf(Statement* stmt) const {
    if (dynamic_cast<ReturnStatement*>(stmt)) return Returns;
    if (dynamic_cast<BreakStatement*>(stmt)) return Breaks;
    if (dynamic_cast<ContinueStatement*>(stmt)) return Continues;
    if (auto* n = dynamic_cast<Block*>(stmt)) return flowOf(n->statements);
    if (auto* n = dynamic_cast<IfStatement*>(stmt))
        return flowOf(n->then_block.get()) |
               (n->else_block ? flowOf(n->else_block.get()) : FallsThrough);
    if (auto* n = dynamic_cast<WhileStatement*>(stmt))
        return loopFlow(n->body.get(), n->condition.get(), false);
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt))
        return loopFlow(n->body.get(), n->condition.get(), true);
    if (auto* n = dynamic_cast<ForStatement*>(stmt))
        return loopFlow(n->body.get(), n->condition.get(), false, true);
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt))
        return (flowOf(n->body.get()) & Returns) | FallsThrough;  // puede estar vacio
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt))
        return flowOf(n->try_block.get()) | flowOf(n->catch_block.get());
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        // Los casos tienen caida al siguiente; cualquier caso puede ser entrada.
        unsigned tail = n->has_default ? flowOf(n->default_statements) : FallsThrough;
        unsigned result = tail;
        for (auto it = n->cases.rbegin(); it != n->cases.rend(); ++it) {
            unsigned current = flowOf((*it)->statements);
            tail = (current & ~FallsThrough) | ((current & FallsThrough) ? tail : 0u);
            result |= tail;
        }
        return result;
    }
    // Declarar una funcion o clase no ejecuta su cuerpo.
    return FallsThrough;
}

void ControlFlowChecker::checkDeadCode(const std::vector<StatementPtr>& statements) {
    unsigned flow = FallsThrough;
    for (auto& stmt : statements) {
        if (!(flow & FallsThrough)) {
            reporter_.error(diagnostics::codes::SEM012,
                             "codigo inalcanzable: nunca se llega a ejecutar esta instruccion.",
                             stmt->line, stmt->column);
            break;  // un diagnostico por lista; el recorrido valida tambien el codigo muerto
        }
        flow = (flow & ~FallsThrough) | flowOf(stmt.get());
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
        if (n->return_type && n->return_type->resolved_type &&
            n->return_type->resolved_type->kind != TypeKind::Void &&
            n->return_type->resolved_type->kind != TypeKind::Error &&
            (flowOf(n->body.get()) & FallsThrough)) {
            reporter_.error(diagnostics::codes::SEM020,
                "la funcion '" + n->name + "' puede terminar sin retornar un valor.",
                n->line, n->column);
        }
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
