#ifndef SQLINT_EXEC_EVAL_CONTEXT_H
#define SQLINT_EXEC_EVAL_CONTEXT_H

#include <map>
#include <string>
#include <vector>
#include "ast/nodes.h"
#include "storage/table_store.h"

namespace sqlint::exec {
class Session;
using Tuple = std::vector<const storage::Row*>;
using AggregateValues = std::map<const ast::Aggregate*, storage::Value>;

struct EvalContext {
    Tuple rows;
    const EvalContext* outer = nullptr;
    const AggregateValues* aggregates = nullptr;
    const storage::Row* output = nullptr;
    Session* session = nullptr;
    std::vector<std::string>* subqueryPlans = nullptr;
};
}  // namespace sqlint::exec
#endif
