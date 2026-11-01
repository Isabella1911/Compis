#include "join.h"
#include "evaluator.h"
namespace sqlint::exec {
std::vector<Tuple> innerJoin(const std::vector<Tuple>& left,
                             const std::vector<storage::StoredRow>& right,
                             const ast::Expr& on, const EvalContext& base) {
    std::vector<Tuple> out;
    EvalContext ctx = base;
    for (const auto& tuple : left) {
        for (const auto& row : right) {
            ctx.rows = tuple; ctx.rows.push_back(&row.values);
            if (isTrue(eval(on, ctx))) out.push_back(ctx.rows);
        }
    }
    return out;
}
}
