#ifndef SQLINT_FRONTEND_AST_BUILDER_H
#define SQLINT_FRONTEND_AST_BUILDER_H

// Convierte el parse tree de ANTLR en el AST propio (ast/nodes.h). Solo lo
// usa parser_driver.cpp, y solo cuando no hubo errores lexicos ni
// sintacticos: con errores, ANTLR deja contextos incompletos (hijos null) y
// el AST no tendria sentido.

#include <memory>

#include "SQLParser.h"
#include "ast/nodes.h"

namespace sqlint {
namespace frontend {

class AstBuilder {
public:
    std::unique_ptr<ast::Script> build(SQLParser::ScriptContext* ctx);

private:
    ast::StatementPtr statement(SQLParser::StatementContext* ctx);
    ast::StatementPtr createTable(SQLParser::CreateTableStmtContext* ctx);
    ast::StatementPtr dropTable(SQLParser::DropTableStmtContext* ctx);
    ast::StatementPtr createIndex(SQLParser::CreateIndexStmtContext* ctx);
    ast::StatementPtr dropIndex(SQLParser::DropIndexStmtContext* ctx);
    ast::StatementPtr insert(SQLParser::InsertStmtContext* ctx);
    ast::StatementPtr update(SQLParser::UpdateStmtContext* ctx);
    ast::StatementPtr deleteStmt(SQLParser::DeleteStmtContext* ctx);

    std::unique_ptr<ast::Select> select(SQLParser::SelectStmtContext* ctx);
    ast::SelectItem selectItem(SQLParser::SelectItemContext* ctx);
    ast::TableRef tableRef(SQLParser::TableRefContext* ctx);
    ast::DataTypeSpec dataType(SQLParser::DataTypeContext* ctx);

    ast::ExprPtr expr(SQLParser::ExprContext* ctx);
    ast::ExprPtr primary(SQLParser::PrimaryContext* ctx);
    ast::ExprPtr aggregate(SQLParser::AggregateContext* ctx);
    ast::ExprPtr literal(SQLParser::LiteralContext* ctx);
    ast::ExprPtr binary(ast::BinaryOp op, SQLParser::ExprContext* left,
                        SQLParser::ExprContext* right, antlr4::Token* opToken,
                        antlr4::ParserRuleContext* whole);
};

}  // namespace frontend
}  // namespace sqlint

#endif  // SQLINT_FRONTEND_AST_BUILDER_H
