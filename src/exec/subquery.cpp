#include "subquery.h"
#include <algorithm>
#include <stdexcept>
#include "session.h"

namespace sqlint::exec {
QueryResult runSubquery(const ast::Select& query, const EvalContext& ctx, bool existenceOnly) {
    if (!ctx.session) throw std::logic_error("Subconsulta sin Session");
    auto& cache = ctx.session->subqueries().cache;
    QueryResult result;
    auto found = cache.find({&query, existenceOnly});
    if (!query.isCorrelated && found != cache.end()) {
        result = found->second;
    } else {
        result = SelectExecutor(*ctx.session).execute(query, &ctx, existenceOnly);
        if (!query.isCorrelated) cache.emplace(std::make_pair(&query, existenceOnly), result);
    }
    if (ctx.subqueryPlans) {
        std::string nestedPlan;
        for (char c : result.plan) {
            nestedPlan += c;
            if (c == '\n') nestedPlan += "  ";
        }
        std::string plan = std::string("Subconsulta (") +
            (query.isCorrelated ? "correlacionada" : "no correlacionada") + "): " + nestedPlan;
        if (std::find(ctx.subqueryPlans->begin(), ctx.subqueryPlans->end(), plan) ==
            ctx.subqueryPlans->end()) ctx.subqueryPlans->push_back(std::move(plan));
    }
    return result;
}
}  // namespace sqlint::exec
