#ifndef SQLINT_EXEC_EVALUATOR_H
#define SQLINT_EXEC_EVALUATOR_H
#include "eval_context.h"

namespace sqlint::exec {
storage::Value eval(const ast::Expr&, const EvalContext&);
bool isTrue(const storage::Value&);
// NULL se ordena como el mayor valor (ASC al final, DESC al inicio).
int compareNullable(const storage::Value&, const storage::Value&);
storage::Value equalValues(storage::Value, storage::Value);
}  // namespace sqlint::exec
#endif
