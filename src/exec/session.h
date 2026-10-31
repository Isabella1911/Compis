#ifndef SQLINT_EXEC_SESSION_H
#define SQLINT_EXEC_SESSION_H
#include <optional>
#include "index/index_manager.h"
#include "subquery.h"

namespace sqlint::exec {
struct StatementResult {
    int index = 0;
    int line = 0;
    std::string kind;
    std::string status = "ok";
    std::vector<ast::OutputColumn> columns;
    std::vector<storage::Row> rows;
    std::optional<int64_t> affected;
    std::string plan;
    std::string message;
};
struct RunOutput {
    std::vector<diagnostics::Diagnostic> diagnostics;
    std::vector<StatementResult> statements;
};
class Session {
public:
    explicit Session(std::string dbDir);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    RunOutput run(const ast::Script&, bool noExec = false);
    storage::Catalog& catalog() { return catalog_; }
    storage::TableStore& store() { return store_; }
    index::IndexManager& indexes() { return indexes_; }
    SubqueryState& subqueries() { return subqueries_; }
private:
    storage::Catalog catalog_;
    storage::TableStore store_;
    index::IndexManager indexes_;
    SubqueryState subqueries_;
};
}  // namespace sqlint::exec
#endif
