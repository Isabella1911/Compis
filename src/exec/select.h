#ifndef SQLINT_EXEC_SELECT_H
#define SQLINT_EXEC_SELECT_H
#include "eval_context.h"

namespace sqlint::exec {
struct QueryResult {
    std::vector<ast::OutputColumn> columns;
    std::vector<storage::Row> rows;
    std::string plan;
};
class SelectExecutor {
public:
    explicit SelectExecutor(Session& session) : session_(session) {}
    QueryResult execute(const ast::Select&, const EvalContext* outer = nullptr,
                        bool existenceOnly = false);
private:
    Session& session_;
};
}  // namespace sqlint::exec
#endif
