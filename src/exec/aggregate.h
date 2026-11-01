#ifndef SQLINT_EXEC_AGGREGATE_H
#define SQLINT_EXEC_AGGREGATE_H
#include "eval_context.h"
namespace sqlint::exec {
struct Group { std::vector<Tuple> rows; };
std::vector<const ast::Aggregate*> collectAggregates(const ast::Select&);
std::vector<const ast::Aggregate*> collectAggregates(const ast::Expr&);
std::vector<Group> groupRows(const std::vector<Tuple>&, const ast::Select&, const EvalContext&);
AggregateValues aggregateValues(const Group&, const std::vector<const ast::Aggregate*>&,
                                const EvalContext&);
}
#endif
