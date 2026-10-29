#include "ast_builder.h"

#include <cerrno>
#include <cstdlib>
#include <stdexcept>

#include "misc/Interval.h"

namespace sqlint {
namespace frontend {

namespace {

ast::SourceLoc locOf(antlr4::Token* token) {
    return {static_cast<int>(token->getLine()),
            static_cast<int>(token->getCharPositionInLine()) + 1};
}

ast::SourceLoc locOf(antlr4::ParserRuleContext* ctx) { return locOf(ctx->getStart()); }

ast::Ident ident(SQLParser::IdentifierContext* ctx) {
    return ast::Ident{ctx->getText(), locOf(ctx)};
}

// Texto original entre el primer y el ultimo token del contexto, con los
// espacios y mayusculas del script ("edad + 1", no "edad+1").
std::string sourceText(antlr4::ParserRuleContext* ctx) {
    antlr4::Token* start = ctx->getStart();
    antlr4::Token* stop = ctx->getStop();
    if (start == nullptr || stop == nullptr) return ctx->getText();
    return start->getInputStream()->getText(
        antlr4::misc::Interval(start->getStartIndex(), stop->getStopIndex()));
}

// 'O''Neil' -> O'Neil
std::string unquote(const std::string& quoted) {
    std::string out;
    for (size_t i = 1; i + 1 < quoted.size(); ++i) {
        out += quoted[i];
        if (quoted[i] == '\'' && quoted[i + 1] == '\'') ++i;
    }
    return out;
}

template <typename T>
std::unique_ptr<T> make(ast::SourceLoc loc) {
    auto node = std::make_unique<T>();
    node->loc = loc;
    return node;
}

}  // namespace

std::unique_ptr<ast::Script> AstBuilder::build(SQLParser::ScriptContext* ctx) {
    auto script = std::make_unique<ast::Script>();
    for (auto* stmt : ctx->statement()) {
        script->statements.push_back(statement(stmt));
    }
    return script;
}

ast::StatementPtr AstBuilder::statement(SQLParser::StatementContext* ctx) {
    if (auto* c = ctx->createTableStmt()) return createTable(c);
    if (auto* c = ctx->dropTableStmt()) return dropTable(c);
    if (auto* c = ctx->createIndexStmt()) return createIndex(c);
    if (auto* c = ctx->dropIndexStmt()) return dropIndex(c);
    if (auto* c = ctx->insertStmt()) return insert(c);
    if (auto* c = ctx->updateStmt()) return update(c);
    if (auto* c = ctx->deleteStmt()) return deleteStmt(c);

    auto stmt = make<ast::SelectStatement>(locOf(ctx));
    stmt->query = select(ctx->selectStmt());
    return stmt;
}

// ---------------------------------------------------------------------------
// DDL
// ---------------------------------------------------------------------------

ast::StatementPtr AstBuilder::createTable(SQLParser::CreateTableStmtContext* ctx) {
    auto stmt = make<ast::CreateTable>(locOf(ctx));
    stmt->table = ident(ctx->identifier());
    for (auto* element : ctx->tableElement()) {
        if (auto* pk = dynamic_cast<SQLParser::PrimaryKeyElementContext*>(element)) {
            stmt->primaryKeyClauses.push_back(ident(pk->identifier()));
            continue;
        }
        auto* col = static_cast<SQLParser::ColumnElementContext*>(element);
        ast::ColumnDef def;
        def.loc = locOf(col);
        def.name = ident(col->identifier());
        def.type = dataType(col->dataType());
        for (auto* constraint : col->columnConstraint()) {
            if (dynamic_cast<SQLParser::NotNullConstraintContext*>(constraint)) {
                def.notNull = true;
            } else {
                def.primaryKey = true;
            }
        }
        stmt->columns.push_back(std::move(def));
    }
    return stmt;
}

ast::DataTypeSpec AstBuilder::dataType(SQLParser::DataTypeContext* ctx) {
    ast::DataTypeSpec spec;
    spec.loc = locOf(ctx);
    if (dynamic_cast<SQLParser::IntTypeContext*>(ctx)) {
        spec.name = ast::TypeName::Int;
    } else if (dynamic_cast<SQLParser::FloatTypeContext*>(ctx)) {
        spec.name = ast::TypeName::Float;
    } else if (dynamic_cast<SQLParser::BooleanTypeContext*>(ctx)) {
        spec.name = ast::TypeName::Boolean;
    } else if (dynamic_cast<SQLParser::DateTypeContext*>(ctx)) {
        spec.name = ast::TypeName::Date;
    } else {
        auto* varchar = static_cast<SQLParser::VarcharTypeContext*>(ctx);
        spec.name = ast::TypeName::Varchar;
        // Un largo absurdo (que no cabe en 64 bits) se deja en -1; el
        // semantico lo reporta igual que VARCHAR(0).
        errno = 0;
        long long n = std::strtoll(varchar->INTEGER_LITERAL()->getText().c_str(), nullptr, 10);
        spec.length = errno == ERANGE ? -1 : n;
    }
    return spec;
}

ast::StatementPtr AstBuilder::dropTable(SQLParser::DropTableStmtContext* ctx) {
    auto stmt = make<ast::DropTable>(locOf(ctx));
    stmt->table = ident(ctx->identifier());
    return stmt;
}

ast::StatementPtr AstBuilder::createIndex(SQLParser::CreateIndexStmtContext* ctx) {
    auto stmt = make<ast::CreateIndex>(locOf(ctx));
    stmt->index = ident(ctx->identifier(0));
    stmt->table = ident(ctx->identifier(1));
    stmt->column = ident(ctx->identifier(2));
    return stmt;
}

ast::StatementPtr AstBuilder::dropIndex(SQLParser::DropIndexStmtContext* ctx) {
    auto stmt = make<ast::DropIndex>(locOf(ctx));
    stmt->index = ident(ctx->identifier());
    return stmt;
}

// ---------------------------------------------------------------------------
// DML
// ---------------------------------------------------------------------------

ast::StatementPtr AstBuilder::insert(SQLParser::InsertStmtContext* ctx) {
    auto stmt = make<ast::Insert>(locOf(ctx));
    auto names = ctx->identifier();  // [tabla, col1, col2, ...]
    stmt->table = ident(names[0]);
    stmt->explicitColumns = names.size() > 1;
    for (size_t i = 1; i < names.size(); ++i) {
        stmt->columns.push_back(ident(names[i]));
    }
    for (auto* row : ctx->valuesRow()) {
        std::vector<ast::ExprPtr> values;
        for (auto* value : row->expr()) {
            values.push_back(expr(value));
        }
        stmt->rows.push_back(std::move(values));
        stmt->rowLocs.push_back(locOf(row));
    }
    return stmt;
}

ast::StatementPtr AstBuilder::update(SQLParser::UpdateStmtContext* ctx) {
    auto stmt = make<ast::Update>(locOf(ctx));
    stmt->table = ident(ctx->identifier());
    for (auto* assignment : ctx->assignment()) {
        stmt->assignments.push_back(
            ast::Assignment{ident(assignment->identifier()), expr(assignment->expr())});
    }
    if (auto* where = ctx->whereClause()) stmt->where = expr(where->expr());
    return stmt;
}

ast::StatementPtr AstBuilder::deleteStmt(SQLParser::DeleteStmtContext* ctx) {
    auto stmt = make<ast::Delete>(locOf(ctx));
    stmt->table = ident(ctx->identifier());
    if (auto* where = ctx->whereClause()) stmt->where = expr(where->expr());
    return stmt;
}

// ---------------------------------------------------------------------------
// SELECT
// ---------------------------------------------------------------------------

std::unique_ptr<ast::Select> AstBuilder::select(SQLParser::SelectStmtContext* ctx) {
    auto query = make<ast::Select>(locOf(ctx));
    for (auto* item : ctx->selectItem()) {
        query->items.push_back(selectItem(item));
    }
    query->from = tableRef(ctx->tableRef());
    for (auto* join : ctx->joinClause()) {
        query->joins.push_back(ast::Join{tableRef(join->tableRef()), expr(join->expr()),
                                         locOf(join)});
    }
    if (auto* where = ctx->whereClause()) query->where = expr(where->expr());
    if (auto* groupBy = ctx->groupByClause()) {
        for (auto* e : groupBy->expr()) query->groupBy.push_back(expr(e));
    }
    if (auto* having = ctx->havingClause()) query->having = expr(having->expr());
    if (auto* orderBy = ctx->orderByClause()) {
        for (auto* item : orderBy->orderItem()) {
            query->orderBy.push_back(
                ast::OrderItem{expr(item->expr()), item->DESC() != nullptr, locOf(item)});
        }
    }
    return query;
}

ast::SelectItem AstBuilder::selectItem(SQLParser::SelectItemContext* ctx) {
    ast::SelectItem item;
    item.loc = locOf(ctx);
    item.text = sourceText(ctx);
    if (dynamic_cast<SQLParser::AllColumnsItemContext*>(ctx)) {
        item.kind = ast::SelectItemKind::AllColumns;
    } else if (auto* t = dynamic_cast<SQLParser::TableColumnsItemContext*>(ctx)) {
        item.kind = ast::SelectItemKind::TableColumns;
        item.table = ident(t->identifier());
    } else {
        auto* e = static_cast<SQLParser::ExprItemContext*>(ctx);
        item.kind = ast::SelectItemKind::Expr;
        item.expr = expr(e->expr());
        item.text = sourceText(e->expr());  // sin el alias
        if (e->identifier() != nullptr) item.alias = ident(e->identifier());
    }
    return item;
}

ast::TableRef AstBuilder::tableRef(SQLParser::TableRefContext* ctx) {
    ast::TableRef ref;
    ref.loc = locOf(ctx);
    auto names = ctx->identifier();
    ref.name = ident(names[0]);
    if (names.size() > 1) ref.alias = ident(names[1]);
    return ref;
}

// ---------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------

ast::ExprPtr AstBuilder::binary(ast::BinaryOp op, SQLParser::ExprContext* left,
                                SQLParser::ExprContext* right, antlr4::Token* opToken,
                                antlr4::ParserRuleContext* whole) {
    auto node = make<ast::Binary>(locOf(whole));
    node->op = op;
    node->left = expr(left);
    node->right = expr(right);
    node->opLoc = locOf(opToken);
    return node;
}

ast::ExprPtr AstBuilder::expr(SQLParser::ExprContext* ctx) {
    using P = SQLParser;

    if (auto* c = dynamic_cast<P::PrimaryExprContext*>(ctx)) {
        return primary(c->primary());
    }
    if (auto* c = dynamic_cast<P::UnaryExprContext*>(ctx)) {
        auto node = make<ast::Unary>(locOf(ctx));
        node->op = c->op->getType() == P::MINUS ? ast::UnaryOp::Neg : ast::UnaryOp::Plus;
        node->operand = expr(c->expr());
        return node;
    }
    if (auto* c = dynamic_cast<P::NotExprContext*>(ctx)) {
        auto node = make<ast::Unary>(locOf(ctx));
        node->op = ast::UnaryOp::Not;
        node->operand = expr(c->expr());
        return node;
    }
    if (auto* c = dynamic_cast<P::MulExprContext*>(ctx)) {
        ast::BinaryOp op = c->op->getType() == P::STAR    ? ast::BinaryOp::Mul
                           : c->op->getType() == P::SLASH ? ast::BinaryOp::Div
                                                          : ast::BinaryOp::Mod;
        return binary(op, c->expr(0), c->expr(1), c->op, ctx);
    }
    if (auto* c = dynamic_cast<P::AddExprContext*>(ctx)) {
        ast::BinaryOp op = c->op->getType() == P::PLUS ? ast::BinaryOp::Add : ast::BinaryOp::Sub;
        return binary(op, c->expr(0), c->expr(1), c->op, ctx);
    }
    if (auto* c = dynamic_cast<P::CompareExprContext*>(ctx)) {
        ast::BinaryOp op;
        switch (c->op->getType()) {
            case P::EQ: op = ast::BinaryOp::Eq; break;
            case P::NEQ: op = ast::BinaryOp::Neq; break;  // <> y != son el mismo token
            case P::LT: op = ast::BinaryOp::Lt; break;
            case P::LE: op = ast::BinaryOp::Le; break;
            case P::GT: op = ast::BinaryOp::Gt; break;
            default: op = ast::BinaryOp::Ge; break;
        }
        return binary(op, c->expr(0), c->expr(1), c->op, ctx);
    }
    if (auto* c = dynamic_cast<P::AndExprContext*>(ctx)) {
        return binary(ast::BinaryOp::And, c->expr(0), c->expr(1), c->AND()->getSymbol(), ctx);
    }
    if (auto* c = dynamic_cast<P::OrExprContext*>(ctx)) {
        return binary(ast::BinaryOp::Or, c->expr(0), c->expr(1), c->OR()->getSymbol(), ctx);
    }
    if (auto* c = dynamic_cast<P::IsNullExprContext*>(ctx)) {
        auto node = make<ast::IsNull>(locOf(ctx));
        node->operand = expr(c->expr());
        node->negated = c->NOT() != nullptr;
        return node;
    }
    if (auto* c = dynamic_cast<P::InSubqueryExprContext*>(ctx)) {
        auto node = make<ast::InSubquery>(locOf(ctx));
        node->operand = expr(c->expr());
        node->subquery = select(c->selectStmt());
        node->negated = c->NOT() != nullptr;
        return node;
    }
    if (auto* c = dynamic_cast<P::InListExprContext*>(ctx)) {
        auto node = make<ast::InList>(locOf(ctx));
        auto exprs = c->expr();  // [operando, item1, item2, ...]
        node->operand = expr(exprs[0]);
        for (size_t i = 1; i < exprs.size(); ++i) node->items.push_back(expr(exprs[i]));
        node->negated = c->NOT() != nullptr;
        return node;
    }
    if (auto* c = dynamic_cast<P::ExistsExprContext*>(ctx)) {
        auto node = make<ast::Exists>(locOf(ctx));
        node->subquery = select(c->selectStmt());
        node->negated = c->NOT() != nullptr;
        return node;
    }
    throw std::logic_error("AstBuilder: alternativa de expr sin manejar: " + ctx->getText());
}

ast::ExprPtr AstBuilder::primary(SQLParser::PrimaryContext* ctx) {
    using P = SQLParser;

    if (auto* c = dynamic_cast<P::LiteralPrimaryContext*>(ctx)) return literal(c->literal());
    if (auto* c = dynamic_cast<P::AggregatePrimaryContext*>(ctx)) return aggregate(c->aggregate());
    if (auto* c = dynamic_cast<P::QualifiedColumnPrimaryContext*>(ctx)) {
        auto node = make<ast::ColumnRef>(locOf(ctx));
        node->table = ident(c->identifier(0));
        node->column = ident(c->identifier(1));
        return node;
    }
    if (auto* c = dynamic_cast<P::ColumnPrimaryContext*>(ctx)) {
        auto node = make<ast::ColumnRef>(locOf(ctx));
        node->column = ident(c->identifier());
        return node;
    }
    if (auto* c = dynamic_cast<P::ScalarSubqueryPrimaryContext*>(ctx)) {
        auto node = make<ast::ScalarSubquery>(locOf(ctx));
        node->subquery = select(c->selectStmt());
        return node;
    }
    // ( expr ): los parentesis solo agrupan, no dejan nodo.
    return expr(static_cast<P::ParenPrimaryContext*>(ctx)->expr());
}

ast::ExprPtr AstBuilder::aggregate(SQLParser::AggregateContext* ctx) {
    using P = SQLParser;

    auto node = make<ast::Aggregate>(locOf(ctx));
    if (dynamic_cast<P::CountStarAggregateContext*>(ctx)) {
        node->func = ast::AggFunc::Count;
        node->star = true;
        return node;
    }
    auto* c = static_cast<P::CallAggregateContext*>(ctx);
    switch (c->fn->getType()) {
        case P::COUNT: node->func = ast::AggFunc::Count; break;
        case P::SUM: node->func = ast::AggFunc::Sum; break;
        case P::AVG: node->func = ast::AggFunc::Avg; break;
        case P::MIN: node->func = ast::AggFunc::Min; break;
        default: node->func = ast::AggFunc::Max; break;
    }
    node->arg = expr(c->expr());
    return node;
}

ast::ExprPtr AstBuilder::literal(SQLParser::LiteralContext* ctx) {
    using P = SQLParser;

    auto node = make<ast::Literal>(locOf(ctx));
    node->text = sourceText(ctx);
    if (auto* c = dynamic_cast<P::IntLiteralContext*>(ctx)) {
        node->literalKind = ast::LiteralKind::Int;
        errno = 0;
        node->intValue = std::strtoll(c->getText().c_str(), nullptr, 10);
        node->intOverflow = errno == ERANGE;
    } else if (auto* c = dynamic_cast<P::FloatLiteralContext*>(ctx)) {
        node->literalKind = ast::LiteralKind::Float;
        node->floatValue = std::strtod(c->getText().c_str(), nullptr);
    } else if (auto* c = dynamic_cast<P::StringLiteralContext*>(ctx)) {
        node->literalKind = ast::LiteralKind::String;
        node->stringValue = unquote(c->STRING_LITERAL()->getText());
    } else if (auto* c = dynamic_cast<P::DateLiteralContext*>(ctx)) {
        // El formato de la fecha lo valida el semantico (SEM011).
        node->literalKind = ast::LiteralKind::Date;
        node->stringValue = unquote(c->STRING_LITERAL()->getText());
    } else if (auto* c = dynamic_cast<P::BoolLiteralContext*>(ctx)) {
        node->literalKind = ast::LiteralKind::Bool;
        node->boolValue = c->TRUE() != nullptr;
    } else {
        node->literalKind = ast::LiteralKind::Null;
    }
    return node;
}

}  // namespace frontend
}  // namespace sqlint
