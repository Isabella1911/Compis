#ifndef SQLINT_EXEC_SUBQUERY_H
#define SQLINT_EXEC_SUBQUERY_H
#include "select.h"

namespace sqlint::exec {
// Vive durante una sentencia: una escritura posterior invalida toda la cache.
struct SubqueryState {
    std::map<std::pair<const ast::Select*, bool>, QueryResult> cache;
};
QueryResult runSubquery(const ast::Select&, const EvalContext&, bool existenceOnly = false);
}  // namespace sqlint::exec
#endif
