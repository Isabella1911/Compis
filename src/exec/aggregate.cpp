#include "aggregate.h"
#include <algorithm>
#include "diagnostics/codes.h"
#include "evaluator.h"
#include "ast/expression_utils.h"

namespace sqlint::exec {
using namespace storage;
namespace {
void collect(const ast::Expr& e, std::vector<const ast::Aggregate*>& out) {
    if (e.kind == ast::ExprKind::Aggregate) {
        const auto* a = &static_cast<const ast::Aggregate&>(e);
        if (std::find(out.begin(), out.end(), a) == out.end()) out.push_back(a);
        return;
    }
    forEachChild(e, [&](const ast::Expr& c) { collect(c, out); });
}
struct KeyLess {
    bool operator()(const Row& a, const Row& b) const {
        for (size_t i = 0; i < a.size(); ++i) {
            int c = compareNullable(a[i], b[i]); if (c) return c < 0;
        }
        return false;
    }
};
}
std::vector<const ast::Aggregate*> collectAggregates(const ast::Expr& e) {
    std::vector<const ast::Aggregate*> out; collect(e, out); return out;
}
std::vector<const ast::Aggregate*> collectAggregates(const ast::Select& q) {
    std::vector<const ast::Aggregate*> out;
    for (const auto& item : q.items) if (item.expr) collect(*item.expr, out);
    if (q.having) collect(*q.having, out);
    for (const auto& item : q.orderBy) collect(*item.expr, out);
    return out;
}
std::vector<Group> groupRows(const std::vector<Tuple>& rows, const ast::Select& q,
                             const EvalContext& base) {
    // Sin GROUP BY, aun una entrada vacia produce el grupo global.
    if (q.groupBy.empty()) return {Group{rows}};
    std::map<Row, Group, KeyLess> groups;
    EvalContext ctx = base;
    for (const auto& tuple : rows) {
        ctx.rows = tuple;
        Row key; for (const auto& expr : q.groupBy) key.push_back(eval(*expr, ctx));
        groups[key].rows.push_back(tuple);
    }
    std::vector<Group> out;
    for (auto& entry : groups) out.push_back(std::move(entry.second));
    return out;
}
AggregateValues aggregateValues(const Group& group, const std::vector<const ast::Aggregate*>& aggs,
                                const EvalContext& base) {
    AggregateValues out;
    for (const auto* a : aggs) {
        int64_t count = 0;
        Value result;
        long double total = 0;
        int64_t integerTotal = 0;
        EvalContext ctx = base;
        for (const auto& tuple : group.rows) {
            ctx.rows = tuple;
            Value v = a->star ? Value::ofInt(1) : eval(*a->arg, ctx);
            if (v.isNull()) continue;
            ++count;
            switch (a->func) {
                case ast::AggFunc::Count: break;
                case ast::AggFunc::Sum:
                case ast::AggFunc::Avg:
                    if (a->func == ast::AggFunc::Sum && a->type.kind == TypeKind::Int) {
                        if (__builtin_add_overflow(integerTotal, v.asInt(), &integerTotal))
                            throw diagnostics::SqlError({diagnostics::Phase::Ejecucion,
                                diagnostics::codes::SEM007, a->loc, "La suma no cabe en un INT (64 bits)"});
                    } else total += v.kind == TypeKind::Int ? static_cast<long double>(v.asInt()) : v.asFloat();
                    break;
                case ast::AggFunc::Min:
                    if (result.isNull() || compareValues(v, result) < 0) result = v;
                    break;
                case ast::AggFunc::Max:
                    if (result.isNull() || compareValues(v, result) > 0) result = v;
                    break;
            }
        }
        if (a->func == ast::AggFunc::Count) result = Value::ofInt(count);
        else if (count && a->func == ast::AggFunc::Avg)
            result = Value::ofFloat(static_cast<double>(total / count));
        else if (count && a->func == ast::AggFunc::Sum)
            result = a->type.kind == TypeKind::Int ? Value::ofInt(integerTotal)
                                                   : Value::ofFloat(static_cast<double>(total));
        out.emplace(a, std::move(result));
    }
    return out;
}
}  // namespace sqlint::exec
