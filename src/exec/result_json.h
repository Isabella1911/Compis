#ifndef SQLINT_EXEC_RESULT_JSON_H
#define SQLINT_EXEC_RESULT_JSON_H
#include "session.h"
namespace sqlint::exec {
std::string valueToJson(const storage::Value&);
std::string diagnosticsToJson(const std::vector<diagnostics::Diagnostic>&);
std::string statementsToJson(const std::vector<StatementResult>&);
std::string toJson(const RunOutput&);
std::string formatTable(const StatementResult&);
}
#endif
