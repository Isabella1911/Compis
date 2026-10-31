#include "select.h"
#include <algorithm>
#include <stdexcept>
#include "aggregate.h"
#include "evaluator.h"
#include "join.h"
#include "planner.h"
#include "session.h"

namespace sqlint::exec {
QueryResult SelectExecutor::execute(const ast::Select& q, const EvalContext* outer, bool existenceOnly) {
    QueryResult result; result.columns = q.outputColumns;
    std::vector<std::string> subPlans;
    EvalContext base; base.outer = outer; base.session = &session_; base.subqueryPlans = &subPlans;
    std::vector<const ast::Expr*> terms, pushed;
    splitConjunctions(q.where.get(), terms);
    for (const auto* term : terms) if (usesOnlyFirstSource(*term)) pushed.push_back(term);
    const auto& from = *session_.catalog().findTable(q.from.name.name);
    auto scan = matchingRows(from, pushed, session_, outer, &subPlans);
    result.plan = scan.plan;

    // Las filas se mantienen vivas hasta terminar proyeccion y ordenamiento.
    std::vector<std::vector<storage::StoredRow>> sources;
    sources.reserve(q.joins.size() + 1);
    sources.push_back(std::move(scan.rows));
    std::vector<Tuple> tuples;
    for (const auto& row : sources.front()) tuples.push_back({&row.values});
    for (const auto& join : q.joins) {
        const auto& table = *session_.catalog().findTable(join.table.name.name);
        sources.push_back(session_.store().scan(table));
        tuples = innerJoin(tuples, sources.back(), *join.on, base);
        result.plan += "\n  NestedLoop INNER JOIN: SeqScan en " + table.name;
    }
    if (!q.joins.empty() && q.where) {
        std::vector<Tuple> filtered;
        for (const auto& tuple : tuples) {
            EvalContext ctx = base; ctx.rows = tuple;
            if (isTrue(eval(*q.where, ctx))) filtered.push_back(tuple);
        }
        tuples = std::move(filtered);
    }

    struct Projected { storage::Row row, keys; };
    std::vector<Projected> projected;
    auto project = [&](EvalContext ctx) {
        Projected p;
        // EXISTS solo necesita cardinalidad: no evalua proyeccion ni ORDER BY.
        if (existenceOnly) { projected.push_back(std::move(p)); return; }
        for (const auto& item : q.items) {
            if (item.kind == ast::SelectItemKind::Expr) p.row.push_back(eval(*item.expr, ctx));
            else if (item.kind == ast::SelectItemKind::AllColumns) {
                for (const auto* row : ctx.rows) p.row.insert(p.row.end(), row->begin(), row->end());
            } else {
                if (item.source < 0) throw std::logic_error("t.* sin anotación semántica");
                const auto* row = ctx.rows.at(item.source);
                p.row.insert(p.row.end(), row->begin(), row->end());
            }
        }
        ctx.output = &p.row;
        for (const auto& order : q.orderBy) p.keys.push_back(eval(*order.expr, ctx));
        projected.push_back(std::move(p));
    };
    auto aggs = collectAggregates(q);
    if (!q.groupBy.empty() || !aggs.empty() || q.having) {
        auto groups = groupRows(tuples, q, base);
        if (existenceOnly) aggs = q.having ? collectAggregates(*q.having) : std::vector<const ast::Aggregate*>{};
        for (const auto& group : groups) {
            auto values = aggregateValues(group, aggs, base);
            EvalContext ctx = base;
            if (!group.rows.empty()) ctx.rows = group.rows.front();
            ctx.aggregates = &values;
            if (!q.having || isTrue(eval(*q.having, ctx))) {
                project(ctx); if (existenceOnly) break;
            }
        }
    } else {
        for (const auto& tuple : tuples) {
            EvalContext ctx = base; ctx.rows = tuple; project(ctx);
            if (existenceOnly) break;
        }
    }
    if (!existenceOnly) std::stable_sort(projected.begin(), projected.end(), [&](const Projected& a, const Projected& b) {
        for (size_t i = 0; i < q.orderBy.size(); ++i) {
            int c = compareNullable(a.keys[i], b.keys[i]);
            if (c) return q.orderBy[i].descending ? c > 0 : c < 0;
        }
        return false;
    });
    for (auto& p : projected) result.rows.push_back(std::move(p.row));
    for (const auto& plan : subPlans) result.plan += "\n  └ " + plan;
    return result;
}
}  // namespace sqlint::exec
