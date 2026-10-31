#ifndef SQLINT_EXEC_PLANNER_H
#define SQLINT_EXEC_PLANNER_H
#include "eval_context.h"

namespace sqlint::exec {
struct ScanResult { std::vector<storage::StoredRow> rows; std::string plan; };
ScanResult matchingRows(const storage::TableDef&, const ast::Expr*, Session&,
                        const EvalContext* outer = nullptr,
                        std::vector<std::string>* subqueryPlans = nullptr);
ScanResult matchingRows(const storage::TableDef&, const std::vector<const ast::Expr*>&, Session&,
                        const EvalContext* outer = nullptr,
                        std::vector<std::string>* subqueryPlans = nullptr);
void splitConjunctions(const ast::Expr*, std::vector<const ast::Expr*>&);
// Incluye dependencias que pasan por subconsultas anidadas.
bool usesOnlyFirstSource(const ast::Expr&);
}  // namespace sqlint::exec
#endif
