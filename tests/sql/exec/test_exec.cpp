// Contratos que necesitan inspeccion del contexto o de la cache de sentencia.
#include <stdexcept>
#include "../storage/check.h"
#include "diagnostics/reporter.h"
#include "exec/evaluator.h"
#include "exec/session.h"
#include "frontend/parser_driver.h"
#include "semantic/analyzer.h"

using namespace sqlint;
using namespace sqlint::storage;

namespace {
std::unique_ptr<ast::Script> parse(const std::string& source) {
    diagnostics::DiagnosticReporter reporter;
    auto parsed = frontend::parse(source, reporter);
    if (!parsed.script) throw std::runtime_error("SQL de prueba invalido");
    return std::move(parsed.script);
}

void bindingContract() {
    Row first{Value::ofInt(10)}, second{Value::ofInt(20)}, third{Value::ofInt(30)};
    exec::EvalContext a; a.rows = {&first};
    exec::EvalContext b; b.rows = {&second}; b.outer = &a;
    exec::EvalContext c; c.rows = {&third, &second}; c.outer = &b;
    ast::ColumnRef ref; ref.column.name = "nombre_que_no_se_resuelve";
    ref.binding = {0, 1, 0, -1};
    CHECK(exec::eval(ref, c).asInt() == 20);
    ref.binding = {2, 0, 0, -1};
    CHECK(exec::eval(ref, c).asInt() == 10);
    ref.binding = {};
    bool rejected = false;
    try { exec::eval(ref, c); } catch (const std::logic_error&) { rejected = true; }
    CHECK(rejected);  // No hay fallback a busqueda por nombre.
    CHECK(!exec::isTrue(Value::null()) && !exec::isTrue(Value::ofInt(1)));
}

void cacheContract() {
    exec::Session session(check::tempDir("exec_cache"));
    auto setup = parse("CREATE TABLE t (id INT); CREATE TABLE u (id INT);"
                       "INSERT INTO t VALUES (1),(2); INSERT INTO u VALUES (1),(2);");
    CHECK(session.run(*setup).diagnostics.empty());
    auto script = parse("SELECT (SELECT id FROM u WHERE id=1) FROM t;");
    CHECK(semantic::analyze(*script->statements[0], session.catalog()).empty());
    auto& query = *static_cast<ast::SelectStatement&>(*script->statements[0]).query;
    auto& sub = *static_cast<ast::ScalarSubquery&>(*query.items[0].expr).subquery;
    exec::EvalContext ctx; ctx.session = &session;
    auto initial = exec::runSubquery(sub, ctx);
    CHECK(initial.rows.size() == 1 && initial.rows[0][0].asInt() == 1);
    CHECK(session.subqueries().cache.size() == 1);
    // Una segunda evaluacion del mismo nodo dentro de la sentencia usa cache.
    session.store().rewrite(*session.catalog().findTable("u"), {{Value::ofInt(2)}});
    auto cached = exec::runSubquery(sub, ctx);
    CHECK(cached.rows.size() == 1 && cached.rows[0][0].asInt() == 1);
    auto rerun = session.run(*script);
    CHECK(rerun.diagnostics.empty());
    CHECK(rerun.statements[0].rows.size() == 2 && rerun.statements[0].rows[0][0].isNull());

    script = parse("SELECT (SELECT id FROM u WHERE u.id=t.id) FROM t ORDER BY id;");
    auto correlated = session.run(*script);
    CHECK(correlated.diagnostics.empty());
    CHECK(correlated.statements[0].rows[0][0].isNull() && correlated.statements[0].rows[1][0].asInt() == 2);
    CHECK(session.subqueries().cache.empty());
}
}

int main() {
    bindingContract(); cacheContract();
    return check::summary("exec");
}
