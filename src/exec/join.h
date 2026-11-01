#ifndef SQLINT_EXEC_JOIN_H
#define SQLINT_EXEC_JOIN_H
#include "eval_context.h"
namespace sqlint::exec {
std::vector<Tuple> innerJoin(const std::vector<Tuple>&, const std::vector<storage::StoredRow>&,
                             const ast::Expr& on, const EvalContext&);
}
#endif
