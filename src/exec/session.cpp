#include "session.h"
#include "evaluator.h"
#include "planner.h"
#include "semantic/analyzer.h"
#include "semantic/script_checker.h"
#include "storage/mutator.h"

namespace sqlint::exec {
Session::Session(std::string dbDir)
    : catalog_(storage::Catalog::load(dbDir)), store_(std::move(dbDir)), indexes_(catalog_, store_) {}

RunOutput Session::run(const ast::Script& script, bool noExec) {
    RunOutput out;
    storage::Catalog simulated = catalog_;
    for (size_t i = 0; i < script.statements.size(); ++i) {
        auto& stmt = *script.statements[i];
        auto diagnostics = semantic::analyze(stmt, noExec ? simulated : catalog_);
        if (!diagnostics.empty()) { out.diagnostics = std::move(diagnostics); break; }
        StatementResult result;
        result.index = static_cast<int>(i); result.line = stmt.loc.line;
        result.kind = ast::statementKindName(stmt.kind);
        subqueries_.cache.clear();
        try {
            if (noExec) {
                semantic::simulateSchema(stmt, simulated);
                continue;
            }
            if (stmt.kind == ast::StatementKind::Select) {
                auto query = SelectExecutor(*this).execute(*static_cast<const ast::SelectStatement&>(stmt).query);
                result.columns = std::move(query.columns); result.rows = std::move(query.rows);
                result.plan = std::move(query.plan);
                result.message = std::to_string(result.rows.size()) + " filas";
            } else {
                std::vector<std::string> subPlans;
                storage::ExecHooks hooks;
                hooks.eval = [&](const ast::Expr& expr, const storage::TableDef&, const storage::Row* row) {
                    EvalContext ctx; ctx.session = this; ctx.subqueryPlans = &subPlans;
                    if (row) ctx.rows = {row};
                    return eval(expr, ctx);
                };
                hooks.matchingRows = [&](const storage::TableDef& table, const ast::Expr* where) {
                    auto matched = matchingRows(table, where, *this, nullptr, &subPlans);
                    result.plan = std::move(matched.plan); return std::move(matched.rows);
                };
                auto mutation = storage::Mutator(catalog_, store_, indexes_, std::move(hooks)).execute(stmt);
                result.affected = mutation.affected; result.message = std::move(mutation.message);
                for (const auto& plan : subPlans) result.plan += "\n  └ " + plan;
                if (stmt.kind == ast::StatementKind::CreateTable || stmt.kind == ast::StatementKind::DropTable ||
                    stmt.kind == ast::StatementKind::CreateIndex || stmt.kind == ast::StatementKind::DropIndex)
                    catalog_ = storage::Catalog::load(catalog_.dbDir());
            }
            out.statements.push_back(std::move(result));
        } catch (const diagnostics::SqlError& e) {
            result.status = "error"; result.message = e.diag.message;
            out.statements.push_back(std::move(result));
            out.diagnostics.push_back(e.diag); break;
        }
    }
    return out;
}
}  // namespace sqlint::exec
