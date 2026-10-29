#include "analyzer.h"

#include <optional>
#include <string>

#include "diagnostics/codes.h"
#include "diagnostics/reporter.h"
#include "scope.h"
#include "type_rules.h"

namespace sqlint {
namespace semantic {

namespace {

namespace codes = diagnostics::codes;
using storage::SqlType;
using storage::TypeKind;
using storage::typeName;

// Largo maximo aceptado en VARCHAR(n). Mas alla es casi seguro un error de
// tipeo, y la Parte 2 no tiene que lidiar con largos absurdos.
constexpr int64_t kMaxVarcharLength = 65535;

// Caracteres (no bytes) de un texto UTF-8: 'año' tiene 3.
int utf8Length(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++n;
    }
    return n;
}

std::string quoted(const std::string& name) { return "'" + name + "'"; }

std::string joinNames(const std::vector<std::string>& names) {
    std::string out;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) out += ", ";
        out += names[i];
    }
    return out;
}

std::string columnList(const storage::TableDef& table) {
    std::vector<std::string> names;
    for (const auto& c : table.columns) names.push_back(c.name);
    return joinNames(names);
}

ast::Literal* asLiteral(ast::Expr& e, ast::LiteralKind kind) {
    if (e.kind != ast::ExprKind::Literal) return nullptr;
    auto* lit = static_cast<ast::Literal*>(&e);
    return lit->literalKind == kind ? lit : nullptr;
}

class Analyzer {
public:
    explicit Analyzer(const storage::Catalog& catalog) : catalog_(catalog) {}

    std::vector<diagnostics::Diagnostic> run(ast::Statement& stmt) {
        switch (stmt.kind) {
            case ast::StatementKind::CreateTable:
                createTable(static_cast<ast::CreateTable&>(stmt));
                break;
            case ast::StatementKind::DropTable:
                dropTable(static_cast<ast::DropTable&>(stmt));
                break;
            case ast::StatementKind::CreateIndex:
            case ast::StatementKind::DropIndex:
                unsupported(stmt.loc, std::string(ast::statementKindName(stmt.kind)));
                break;
            case ast::StatementKind::Insert:
                insert(static_cast<ast::Insert&>(stmt));
                break;
            case ast::StatementKind::Update:
                update(static_cast<ast::Update&>(stmt));
                break;
            case ast::StatementKind::Delete:
                deleteStmt(static_cast<ast::Delete&>(stmt));
                break;
            case ast::StatementKind::Select:
                select(*static_cast<ast::SelectStatement&>(stmt).query);
                break;
        }
        return reporter_.sorted();
    }

private:
    const storage::Catalog& catalog_;
    diagnostics::DiagnosticReporter reporter_;
    ScopeStack scopes_;
    std::vector<ast::Select*> selects_;  // paralelo a scopes_ dentro de un SELECT

    void error(const char* code, ast::SourceLoc loc, const std::string& message, int length = 1) {
        reporter_.error(diagnostics::Phase::Semantico, code, message, loc, length);
    }

    void error(const char* code, const ast::Ident& id, const std::string& message) {
        error(code, id.loc, message, utf8Length(id.name));
    }

    void unsupported(ast::SourceLoc loc, const std::string& what) {
        error(codes::SEM023, loc, what + " todavía no está soportado (llega en la entrega 2)");
    }

    // Busca la tabla y reporta SEM001 si no existe.
    const storage::TableDef* requireTable(const ast::Ident& name) {
        const storage::TableDef* table = catalog_.findTable(name.name);
        if (table == nullptr) {
            std::string msg = "La tabla " + quoted(name.name) + " no existe";
            std::vector<std::string> names;
            for (const auto* t : catalog_.tables()) names.push_back(t->name);
            msg += names.empty() ? " (la base no tiene tablas)"
                                 : ". Tablas disponibles: " + joinNames(names);
            error(codes::SEM001, name, msg);
        }
        return table;
    }

    // -----------------------------------------------------------------------
    // DDL
    // -----------------------------------------------------------------------

    void createTable(ast::CreateTable& stmt) {
        if (catalog_.findTable(stmt.table.name) != nullptr) {
            error(codes::SEM013, stmt.table, "La tabla " + quoted(stmt.table.name) + " ya existe");
        }

        const ast::Ident* firstPk = nullptr;
        auto notePrimaryKey = [&](const ast::Ident& at) {
            if (firstPk == nullptr) {
                firstPk = &at;
                return;
            }
            error(codes::SEM014, at,
                  "Solo se permite una PRIMARY KEY por tabla (ya se declaró en " +
                      quoted(firstPk->name) + ")");
        };

        for (size_t i = 0; i < stmt.columns.size(); ++i) {
            const auto& col = stmt.columns[i];
            for (size_t j = 0; j < i; ++j) {
                if (storage::sameName(stmt.columns[j].name.name, col.name.name)) {
                    error(codes::SEM014, col.name,
                          "La columna " + quoted(col.name.name) + " está repetida");
                    break;
                }
            }
            if (col.type.name == ast::TypeName::Varchar &&
                (col.type.length <= 0 || col.type.length > kMaxVarcharLength)) {
                error(codes::SEM014, col.type.loc,
                      "El largo de VARCHAR en " + quoted(col.name.name) +
                          " debe estar entre 1 y " + std::to_string(kMaxVarcharLength),
                      7);
            }
            if (col.primaryKey) notePrimaryKey(col.name);
        }

        for (const auto& pk : stmt.primaryKeyClauses) {
            bool exists = false;
            for (const auto& col : stmt.columns) {
                if (storage::sameName(col.name.name, pk.name)) exists = true;
            }
            if (!exists) {
                error(codes::SEM002, pk,
                      "La PRIMARY KEY nombra la columna " + quoted(pk.name) +
                          ", que no está en la tabla");
                continue;
            }
            notePrimaryKey(pk);
        }
    }

    void dropTable(ast::DropTable& stmt) { requireTable(stmt.table); }

    // -----------------------------------------------------------------------
    // DML
    // -----------------------------------------------------------------------

    void insert(ast::Insert& stmt) {
        const storage::TableDef* table = requireTable(stmt.table);
        if (table == nullptr) return;

        // Columna destino de cada posicion de VALUES.
        std::vector<int> targets;
        bool columnsOk = true;
        if (stmt.explicitColumns) {
            for (const auto& name : stmt.columns) {
                int idx = table->findColumn(name.name);
                if (idx < 0) {
                    error(codes::SEM002, name,
                          "La columna " + quoted(name.name) + " no existe en " +
                              quoted(table->name) + ". Columnas disponibles: " +
                              columnList(*table));
                    columnsOk = false;
                    continue;
                }
                for (int prev : targets) {
                    if (prev == idx) {
                        error(codes::SEM015, name,
                              "La columna " + quoted(name.name) + " aparece dos veces en el INSERT");
                        columnsOk = false;
                    }
                }
                targets.push_back(idx);
            }
            // Las columnas que no se nombran quedan en NULL.
            for (size_t c = 0; c < table->columns.size() && columnsOk; ++c) {
                const auto& col = table->columns[c];
                bool listed = false;
                for (int t : targets) listed = listed || t == static_cast<int>(c);
                if (!listed && col.notNull) {
                    error(codes::SEM012, stmt.table,
                          "La columna " + quoted(col.name) +
                              " es NOT NULL y el INSERT no le da valor");
                }
            }
        } else {
            for (size_t c = 0; c < table->columns.size(); ++c) {
                stmt.columns.push_back(ast::Ident{table->columns[c].name, stmt.table.loc});
                targets.push_back(static_cast<int>(c));
            }
        }
        if (!columnsOk) return;

        for (size_t r = 0; r < stmt.rows.size(); ++r) {
            auto& row = stmt.rows[r];
            if (row.size() != targets.size()) {
                std::vector<std::string> names;
                for (int t : targets) names.push_back(table->columns[t].name);
                error(codes::SEM008, stmt.rowLocs[r],
                      "La fila tiene " + std::to_string(row.size()) + " valor(es) y se esperaban " +
                          std::to_string(targets.size()) + " (" + joinNames(names) + ")");
                continue;
            }
            for (size_t i = 0; i < row.size(); ++i) {
                // En VALUES no hay alcance: una columna ahi es un error.
                std::optional<SqlType> t = expr(*row[i]);
                checkValueForColumn(*row[i], t, table->columns[targets[i]]);
            }
        }
    }

    void update(ast::Update& stmt) {
        const storage::TableDef* table = requireTable(stmt.table);
        if (table == nullptr) return;

        scopes_.push({Source{table->name, "", table}});
        std::vector<int> assigned;
        for (auto& a : stmt.assignments) {
            int idx = table->findColumn(a.column.name);
            std::optional<SqlType> t = expr(*a.value);
            if (idx < 0) {
                error(codes::SEM002, a.column,
                      "La columna " + quoted(a.column.name) + " no existe en " +
                          quoted(table->name) + ". Columnas disponibles: " + columnList(*table));
                continue;
            }
            for (int prev : assigned) {
                if (prev == idx) {
                    error(codes::SEM015, a.column,
                          "La columna " + quoted(a.column.name) + " aparece dos veces en el SET");
                }
            }
            assigned.push_back(idx);
            checkValueForColumn(*a.value, t, table->columns[idx]);
        }
        if (stmt.where) condition(*stmt.where, "WHERE");
        scopes_.pop();
    }

    void deleteStmt(ast::Delete& stmt) {
        const storage::TableDef* table = requireTable(stmt.table);
        if (table == nullptr) return;

        scopes_.push({Source{table->name, "", table}});
        if (stmt.where) condition(*stmt.where, "WHERE");
        scopes_.pop();
    }

    // Valor que se guarda en una columna (INSERT y SET de UPDATE).
    void checkValueForColumn(ast::Expr& value, std::optional<SqlType> t,
                             const storage::ColumnDef& col) {
        if (!t) return;  // la expresion ya tenia un error

        if (col.type.kind == TypeKind::Date && asLiteral(value, ast::LiteralKind::String)) {
            if (!coerceToDate(value)) return;
            t = value.type;
        }
        if (isNull(*t)) {
            // Solo el NULL literal se detecta aqui; un NULL calculado en
            // ejecucion es RUN102 (Parte 2).
            if (col.notNull && asLiteral(value, ast::LiteralKind::Null)) {
                error(codes::SEM012, value.loc, "La columna " + quoted(col.name) + " es NOT NULL",
                      4);
            }
            return;
        }
        if (!assignable(col.type, *t)) {
            error(codes::SEM009, value.loc,
                  "La columna " + quoted(col.name) + " es " + typeName(col.type) +
                      " y el valor es " + typeName(*t));
            return;
        }
        if (col.type.kind == TypeKind::Varchar) {
            if (auto* lit = asLiteral(value, ast::LiteralKind::String)) {
                int len = utf8Length(lit->stringValue);
                if (len > col.type.length) {
                    error(codes::SEM010, value.loc,
                          "El texto tiene " + std::to_string(len) + " caracteres y la columna " +
                              quoted(col.name) + " es " + typeName(col.type),
                          utf8Length(lit->text));
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // SELECT
    // -----------------------------------------------------------------------

    void select(ast::Select& query) {
        for (const auto& join : query.joins) unsupported(join.loc, "JOIN");
        if (!query.groupBy.empty()) unsupported(query.groupBy[0]->loc, "GROUP BY");
        if (query.having) unsupported(query.having->loc, "HAVING");

        const storage::TableDef* table = requireTable(query.from.name);
        if (table == nullptr) return;

        scopes_.push({Source{table->name, query.from.alias.name, table}});
        selects_.push_back(&query);

        for (auto& item : query.items) selectItem(query, item);
        if (query.where) condition(*query.where, "WHERE");
        for (auto& item : query.orderBy) orderItem(query, item);

        selects_.pop_back();
        scopes_.pop();
    }

    void selectItem(ast::Select& query, ast::SelectItem& item) {
        switch (item.kind) {
            case ast::SelectItemKind::AllColumns:
                for (const auto& source : scopes_.innermost()) {
                    for (const auto& col : source.def->columns) {
                        query.outputColumns.push_back(ast::OutputColumn{col.name, col.type});
                    }
                }
                return;
            case ast::SelectItemKind::TableColumns: {
                int s = scopes_.findSource(item.table.name);
                if (s < 0) {
                    error(codes::SEM004, item.table,
                          quoted(item.table.name) + " no es una tabla ni un alias del FROM" +
                              availableSources());
                    return;
                }
                for (const auto& col : scopes_.innermost()[s].def->columns) {
                    query.outputColumns.push_back(ast::OutputColumn{col.name, col.type});
                }
                return;
            }
            case ast::SelectItemKind::Expr: {
                std::optional<SqlType> t = expr(*item.expr);
                std::string name = item.alias.name;
                if (name.empty() && item.expr->kind == ast::ExprKind::ColumnRef) {
                    name = static_cast<ast::ColumnRef&>(*item.expr).column.name;
                }
                if (name.empty()) name = item.text;
                query.outputColumns.push_back(ast::OutputColumn{name, t ? *t : SqlType{}});
                return;
            }
        }
    }

    // ORDER BY acepta un alias de la lista del SELECT ("ORDER BY total") o
    // cualquier expresion sobre las tablas. Si un nombre es alias y columna a
    // la vez, gana el alias (como en SQL estandar).
    void orderItem(ast::Select& query, ast::OrderItem& item) {
        if (item.expr->kind == ast::ExprKind::ColumnRef) {
            auto& ref = static_cast<ast::ColumnRef&>(*item.expr);
            if (ref.table.empty()) {
                for (size_t i = 0; i < query.items.size(); ++i) {
                    const auto& selItem = query.items[i];
                    if (selItem.kind == ast::SelectItemKind::Expr && !selItem.alias.empty() &&
                        storage::sameName(selItem.alias.name, ref.column.name)) {
                        ref.binding.outputColumn = outputIndexOf(query, i);
                        ref.type = query.outputColumns[ref.binding.outputColumn].type;
                        return;
                    }
                }
            }
        }
        expr(*item.expr);
    }

    // Posicion en outputColumns del item i (los "*" anteriores ocupan varias).
    int outputIndexOf(const ast::Select& query, size_t itemIndex) {
        int index = 0;
        for (size_t i = 0; i < itemIndex; ++i) {
            const auto& item = query.items[i];
            if (item.kind == ast::SelectItemKind::Expr) {
                ++index;
            } else if (item.kind == ast::SelectItemKind::AllColumns) {
                for (const auto& s : scopes_.innermost()) index += s.def->columns.size();
            } else {
                int s = scopes_.findSource(item.table.name);
                if (s >= 0) index += scopes_.innermost()[s].def->columns.size();
            }
        }
        return index;
    }

    std::string availableSources() const {
        if (scopes_.empty()) return "";
        std::vector<std::string> names;
        for (const auto& s : scopes_.innermost()) names.push_back(s.visibleName());
        return ". En el FROM hay: " + joinNames(names);
    }

    // WHERE / ON / HAVING: la condicion tiene que ser booleana.
    void condition(ast::Expr& e, const char* clause) {
        std::optional<SqlType> t = expr(e);
        if (t && !isBooleanLike(*t)) {
            error(codes::SEM007, e.loc,
                  std::string("La condición del ") + clause + " debe ser BOOLEAN, pero es " +
                      typeName(*t));
        }
    }

    // -----------------------------------------------------------------------
    // Expresiones
    // -----------------------------------------------------------------------

    // Devuelve el tipo de la expresion y lo anota en e.type. nullopt si hubo
    // un error adentro: quien llama no reporta nada mas sobre ella, para no
    // encadenar errores que salen todos de la misma causa.
    std::optional<SqlType> expr(ast::Expr& e) {
        std::optional<SqlType> t = exprInner(e);
        if (t) e.type = *t;
        return t;
    }

    std::optional<SqlType> exprInner(ast::Expr& e) {
        switch (e.kind) {
            case ast::ExprKind::Literal: return literal(static_cast<ast::Literal&>(e));
            case ast::ExprKind::ColumnRef: return columnRef(static_cast<ast::ColumnRef&>(e));
            case ast::ExprKind::Unary: return unary(static_cast<ast::Unary&>(e));
            case ast::ExprKind::Binary: return binary(static_cast<ast::Binary&>(e));
            case ast::ExprKind::IsNull: {
                auto& n = static_cast<ast::IsNull&>(e);
                if (!expr(*n.operand)) return std::nullopt;
                return SqlType{TypeKind::Boolean};
            }
            case ast::ExprKind::InList: return inList(static_cast<ast::InList&>(e));
            case ast::ExprKind::InSubquery:
                unsupported(e.loc, "IN con subconsulta");
                return std::nullopt;
            case ast::ExprKind::Exists:
                unsupported(e.loc, "EXISTS");
                return std::nullopt;
            case ast::ExprKind::ScalarSubquery:
                unsupported(e.loc, "La subconsulta escalar");
                return std::nullopt;
            case ast::ExprKind::Aggregate: {
                auto& agg = static_cast<ast::Aggregate&>(e);
                unsupported(e.loc, std::string("El agregado ") + ast::aggFuncName(agg.func));
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<SqlType> literal(ast::Literal& lit) {
        switch (lit.literalKind) {
            case ast::LiteralKind::Int:
                if (lit.intOverflow) {
                    error(codes::SEM007, lit.loc,
                          "El número " + lit.text + " no cabe en un INT (64 bits)",
                          static_cast<int>(lit.text.size()));
                    return std::nullopt;
                }
                return SqlType{TypeKind::Int};
            case ast::LiteralKind::Float: return SqlType{TypeKind::Float};
            case ast::LiteralKind::String:
                return SqlType{TypeKind::Varchar, utf8Length(lit.stringValue)};
            case ast::LiteralKind::Date: {
                storage::Date d;
                if (!storage::parseDate(lit.stringValue, d)) {
                    invalidDate(lit);
                    return std::nullopt;
                }
                return SqlType{TypeKind::Date};
            }
            case ast::LiteralKind::Bool: return SqlType{TypeKind::Boolean};
            case ast::LiteralKind::Null: return SqlType{TypeKind::Null};
        }
        return std::nullopt;
    }

    void invalidDate(const ast::Literal& lit) {
        error(codes::SEM011, lit.loc,
              "Fecha inválida " + quoted(lit.stringValue) +
                  ": se espera YYYY-MM-DD con una fecha que exista",
              utf8Length(lit.text));
    }

    // Una cadena usada donde va una fecha ('2024-01-15' contra una columna
    // DATE) se convierte en literal DATE si es valida. Devuelve false (y
    // reporta SEM011) si no lo es.
    bool coerceToDate(ast::Expr& e) {
        auto* lit = asLiteral(e, ast::LiteralKind::String);
        if (lit == nullptr) return true;
        storage::Date d;
        if (!storage::parseDate(lit->stringValue, d)) {
            invalidDate(*lit);
            return false;
        }
        lit->literalKind = ast::LiteralKind::Date;
        lit->type = SqlType{TypeKind::Date};
        return true;
    }

    std::optional<SqlType> columnRef(ast::ColumnRef& ref) {
        if (scopes_.empty()) {
            error(codes::SEM002, ref.column,
                  "No se pueden usar columnas aquí (" + quoted(ref.column.name) +
                      "): VALUES solo acepta valores");
            return std::nullopt;
        }
        Resolution res = scopes_.resolve(ref.table, ref.column);
        switch (res.result) {
            case LookupResult::Found: {
                ref.binding = res.binding;
                // Columna de una consulta externa: las consultas de por medio
                // dependen de la fila externa (correlacionadas).
                for (int d = 0; d < res.binding.depth && d < static_cast<int>(selects_.size()); ++d) {
                    selects_[selects_.size() - 1 - d]->isCorrelated = true;
                }
                return res.column->type;
            }
            case LookupResult::UnknownQualifier:
                error(codes::SEM004, ref.table,
                      quoted(ref.table.name) + " no es una tabla ni un alias del FROM" +
                          availableSources());
                return std::nullopt;
            case LookupResult::Ambiguous: {
                std::vector<std::string> names;
                for (const auto* s : res.sources) names.push_back(s->visibleName());
                error(codes::SEM003, ref.column,
                      "La columna " + quoted(ref.column.name) + " es ambigua: está en " +
                          joinNames(names) + ". Califícala, por ejemplo " + names[0] + "." +
                          ref.column.name);
                return std::nullopt;
            }
            case LookupResult::NotFound: {
                std::string where;
                std::string available;
                for (const auto* s : res.sources) {
                    where += (where.empty() ? "" : ", ") + quoted(s->table);
                    available += (available.empty() ? "" : "; ") + columnList(*s->def);
                }
                error(codes::SEM002, ref.column,
                      "La columna " + quoted(ref.column.name) + " no existe en " + where +
                          ". Columnas disponibles: " + available);
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<SqlType> unary(ast::Unary& u) {
        std::optional<SqlType> t = expr(*u.operand);
        if (!t) return std::nullopt;
        if (u.op == ast::UnaryOp::Not) {
            if (!isBooleanLike(*t)) {
                error(codes::SEM007, u.loc, "NOT necesita un BOOLEAN, pero recibió " + typeName(*t),
                      3);
                return std::nullopt;
            }
            return SqlType{TypeKind::Boolean};
        }
        if (!isNumeric(*t) && !isNull(*t)) {
            error(codes::SEM007, u.loc,
                  std::string("El signo ") + ast::unaryOpSymbol(u.op) +
                      " necesita un número, pero recibió " + typeName(*t));
            return std::nullopt;
        }
        return *t;
    }

    std::optional<SqlType> binary(ast::Binary& b) {
        std::optional<SqlType> left = expr(*b.left);
        std::optional<SqlType> right = expr(*b.right);
        if (!left || !right) return std::nullopt;

        const std::string op = ast::binaryOpSymbol(b.op);
        const int opLen = static_cast<int>(op.size());
        switch (b.op) {
            case ast::BinaryOp::And:
            case ast::BinaryOp::Or: {
                bool ok = true;
                if (!isBooleanLike(*left)) {
                    error(codes::SEM007, b.left->loc,
                          op + " necesita condiciones BOOLEAN, pero el lado izquierdo es " +
                              typeName(*left));
                    ok = false;
                }
                if (!isBooleanLike(*right)) {
                    error(codes::SEM007, b.right->loc,
                          op + " necesita condiciones BOOLEAN, pero el lado derecho es " +
                              typeName(*right));
                    ok = false;
                }
                if (!ok) return std::nullopt;
                return SqlType{TypeKind::Boolean};
            }
            case ast::BinaryOp::Eq:
            case ast::BinaryOp::Neq:
            case ast::BinaryOp::Lt:
            case ast::BinaryOp::Le:
            case ast::BinaryOp::Gt:
            case ast::BinaryOp::Ge: {
                // fecha < '2020-01-01': la cadena se lee como fecha.
                if (left->kind == TypeKind::Date && right->kind == TypeKind::Varchar) {
                    if (!coerceToDate(*b.right)) return std::nullopt;
                    right = b.right->type;
                } else if (right->kind == TypeKind::Date && left->kind == TypeKind::Varchar) {
                    if (!coerceToDate(*b.left)) return std::nullopt;
                    left = b.left->type;
                }
                if (!comparable(*left, *right)) {
                    error(codes::SEM006, b.opLoc,
                          "No se puede comparar " + typeName(*left) + " con " + typeName(*right) +
                              " usando " + op,
                          opLen);
                    return std::nullopt;
                }
                return SqlType{TypeKind::Boolean};
            }
            default: {  // + - * / %
                SqlType result;
                if (!arithmeticResult(*left, *right, result)) {
                    error(codes::SEM007, b.opLoc,
                          "No se puede aplicar " + op + " entre " + typeName(*left) + " y " +
                              typeName(*right) + ": se necesitan números",
                          opLen);
                    return std::nullopt;
                }
                return result;
            }
        }
    }

    std::optional<SqlType> inList(ast::InList& in) {
        std::optional<SqlType> operand = expr(*in.operand);
        bool ok = operand.has_value();
        for (auto& item : in.items) {
            std::optional<SqlType> t = expr(*item);
            if (!t || !operand) {
                ok = false;
                continue;
            }
            if (operand->kind == TypeKind::Date && t->kind == TypeKind::Varchar) {
                if (!coerceToDate(*item)) {
                    ok = false;
                    continue;
                }
                t = item->type;
            }
            if (!comparable(*operand, *t)) {
                error(codes::SEM006, item->loc,
                      "La lista de IN tiene un " + typeName(*t) + " y se compara con " +
                          typeName(*operand));
                ok = false;
            }
        }
        if (!ok) return std::nullopt;
        return SqlType{TypeKind::Boolean};
    }
};

}  // namespace

std::vector<diagnostics::Diagnostic> analyze(ast::Statement& stmt,
                                             const storage::Catalog& catalog) {
    return Analyzer(catalog).run(stmt);
}

}  // namespace semantic
}  // namespace sqlint
