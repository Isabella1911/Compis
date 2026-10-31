#include "planner.h"
#include "evaluator.h"
#include "ast/expression_utils.h"
#include "session.h"

namespace sqlint::exec {
using namespace storage;
namespace {
// Visita referencias al alcance de la consulta que se está planificando.
// En una subconsulta de nivel n, depth == n apunta a esta consulta.
void queryDependencies(const ast::Select&, int, bool&, bool&);
void dependencies(const ast::Expr& e, int nesting, bool& any, bool& other) {
    if (e.kind == ast::ExprKind::ColumnRef) {
        const auto& b = static_cast<const ast::ColumnRef&>(e).binding;
        if (b.depth == nesting) { any = true; other |= b.source != 0; }
    }
    forEachChild(e, [&](const ast::Expr& c) { dependencies(c, nesting, any, other); });
    const ast::Select* q = nullptr;
    if (e.kind == ast::ExprKind::InSubquery) q = static_cast<const ast::InSubquery&>(e).subquery.get();
    if (e.kind == ast::ExprKind::Exists) q = static_cast<const ast::Exists&>(e).subquery.get();
    if (e.kind == ast::ExprKind::ScalarSubquery) q = static_cast<const ast::ScalarSubquery&>(e).subquery.get();
    if (q) queryDependencies(*q, nesting + 1, any, other);
}
void queryDependencies(const ast::Select& q, int n, bool& any, bool& other) {
    for (const auto& i : q.items) if (i.expr) dependencies(*i.expr, n, any, other);
    for (const auto& j : q.joins) dependencies(*j.on, n, any, other);
    if (q.where) dependencies(*q.where, n, any, other);
    for (const auto& g : q.groupBy) dependencies(*g, n, any, other);
    if (q.having) dependencies(*q.having, n, any, other);
    for (const auto& o : q.orderBy) dependencies(*o.expr, n, any, other);
}
bool constant(const ast::Expr& e) {
    bool any = false, other = false; dependencies(e, 0, any, other); return !any;
}
ast::BinaryOp reverse(ast::BinaryOp op) {
    using O = ast::BinaryOp;
    switch (op) {
        case O::Lt: return O::Gt; case O::Le: return O::Ge;
        case O::Gt: return O::Lt; case O::Ge: return O::Le;
        default: return op;
    }
}
}

bool usesOnlyFirstSource(const ast::Expr& e) {
    bool any = false, other = false; dependencies(e, 0, any, other); return !other;
}
void splitConjunctions(const ast::Expr* e, std::vector<const ast::Expr*>& out) {
    if (!e) return;
    if (e->kind == ast::ExprKind::Binary) {
        const auto& b = static_cast<const ast::Binary&>(*e);
        if (b.op == ast::BinaryOp::And) {
            splitConjunctions(b.left.get(), out); splitConjunctions(b.right.get(), out); return;
        }
    }
    out.push_back(e);
}
ScanResult matchingRows(const TableDef& table, const ast::Expr* where, Session& session,
                        const EvalContext* outer, std::vector<std::string>* subPlans) {
    std::vector<const ast::Expr*> terms; splitConjunctions(where, terms);
    return matchingRows(table, terms, session, outer, subPlans);
}
ScanResult matchingRows(const TableDef& table, const std::vector<const ast::Expr*>& terms,
                        Session& session, const EvalContext* outer,
                        std::vector<std::string>* subPlans) {
    ScanResult result;
    result.plan = "SeqScan en " + table.name;
    EvalContext ctx; ctx.session = &session; ctx.outer = outer; ctx.subqueryPlans = subPlans;
    bool indexed = false;
    using O = ast::BinaryOp;
    for (const auto* term : terms) {
        if (term->kind != ast::ExprKind::Binary) continue;
        const auto& b = static_cast<const ast::Binary&>(*term);
        O op = b.op;
        if (op != O::Eq && op != O::Lt && op != O::Le && op != O::Gt && op != O::Ge) continue;
        const ast::Expr* col = b.left.get(); const ast::Expr* value = b.right.get();
        auto localColumn = [](const ast::Expr* e) {
            if (e->kind != ast::ExprKind::ColumnRef) return false;
            const auto& b = static_cast<const ast::ColumnRef&>(*e).binding;
            return b.depth == 0 && b.source == 0 && b.column >= 0;
        };
        if (!localColumn(col) || !constant(*value)) {
            col = b.right.get(); value = b.left.get(); op = reverse(op);
        }
        if (!localColumn(col) || !constant(*value)) continue;
        int c = static_cast<const ast::ColumnRef&>(*col).binding.column;
        auto indexes = session.catalog().indexesOn(table.name, table.columns.at(c).name);
        if (indexes.empty()) continue;
        const auto& idx = *indexes.front();
        Value v = eval(*value, ctx);
        std::vector<uint64_t> offsets;
        if (op == O::Eq) offsets = session.indexes().lookupEq(idx, v);
        else if (op == O::Gt || op == O::Ge)
            offsets = session.indexes().lookupRange(idx, v, op == O::Ge, std::nullopt, false);
        else offsets = session.indexes().lookupRange(idx, std::nullopt, false, v, op == O::Le);
        result.rows = session.store().readAt(table, offsets);
        result.plan = "IndexScan " + idx.name + " (" + table.columns[c].name + " " +
            ast::binaryOpSymbol(op) + " " + displayValue(v) + ") en " + table.name;
        indexed = true; break;
    }
    if (!indexed) result.rows = session.store().scan(table);
    std::vector<StoredRow> filtered;
    for (auto& row : result.rows) {
        ctx.rows = {&row.values};
        bool keep = true;
        for (const auto* term : terms) if (!isTrue(eval(*term, ctx))) { keep = false; break; }
        if (keep) filtered.push_back(std::move(row));
    }
    result.rows = std::move(filtered);
    return result;
}
}  // namespace sqlint::exec
