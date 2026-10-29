#include "nodes.h"

namespace sqlint {
namespace ast {

const char* binaryOpSymbol(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Mod: return "%";
        case BinaryOp::Eq: return "=";
        case BinaryOp::Neq: return "<>";
        case BinaryOp::Lt: return "<";
        case BinaryOp::Le: return "<=";
        case BinaryOp::Gt: return ">";
        case BinaryOp::Ge: return ">=";
        case BinaryOp::And: return "AND";
        case BinaryOp::Or: return "OR";
    }
    return "?";
}

const char* unaryOpSymbol(UnaryOp op) {
    switch (op) {
        case UnaryOp::Neg: return "-";
        case UnaryOp::Plus: return "+";
        case UnaryOp::Not: return "NOT";
    }
    return "?";
}

const char* aggFuncName(AggFunc f) {
    switch (f) {
        case AggFunc::Count: return "COUNT";
        case AggFunc::Sum: return "SUM";
        case AggFunc::Avg: return "AVG";
        case AggFunc::Min: return "MIN";
        case AggFunc::Max: return "MAX";
    }
    return "?";
}

const char* statementKindName(StatementKind k) {
    switch (k) {
        case StatementKind::CreateTable: return "CREATE TABLE";
        case StatementKind::DropTable: return "DROP TABLE";
        case StatementKind::CreateIndex: return "CREATE INDEX";
        case StatementKind::DropIndex: return "DROP INDEX";
        case StatementKind::Insert: return "INSERT";
        case StatementKind::Update: return "UPDATE";
        case StatementKind::Delete: return "DELETE";
        case StatementKind::Select: return "SELECT";
    }
    return "?";
}

std::string typeSpecText(const DataTypeSpec& t) {
    switch (t.name) {
        case TypeName::Int: return "INT";
        case TypeName::Float: return "FLOAT";
        case TypeName::Varchar: return "VARCHAR(" + std::to_string(t.length) + ")";
        case TypeName::Boolean: return "BOOLEAN";
        case TypeName::Date: return "DATE";
    }
    return "?";
}

}  // namespace ast
}  // namespace sqlint
