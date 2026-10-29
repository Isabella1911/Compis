#include "node_view.h"

namespace sqlint {
namespace ast {

namespace {

ViewNode leaf(std::string label, ViewCategory category, SourceLoc loc) {
    return ViewNode{std::move(label), category, loc, {}};
}

// Nodo de clausula ("WHERE", "GROUP BY", ...) que agrupa a sus hijos.
ViewNode clause(std::string label, SourceLoc loc, std::vector<ViewNode> children) {
    return ViewNode{std::move(label), ViewCategory::Clause, loc, std::move(children)};
}

std::string qualified(const Ident& table, const Ident& column) {
    return table.empty() ? column.name : table.name + "." + column.name;
}

std::string tableRefText(const TableRef& ref) {
    return ref.alias.empty() ? ref.name.name : ref.name.name + " AS " + ref.alias.name;
}

ViewNode viewOf(const Select& select);

ViewNode viewOf(const Expr& e) {
    switch (e.kind) {
        case ExprKind::Literal: {
            const auto& lit = static_cast<const Literal&>(e);
            return leaf("Literal(" + lit.text + ")", ViewCategory::Literal, e.loc);
        }
        case ExprKind::ColumnRef: {
            const auto& col = static_cast<const ColumnRef&>(e);
            return leaf("ColumnRef(" + qualified(col.table, col.column) + ")",
                        ViewCategory::Column, e.loc);
        }
        case ExprKind::Unary: {
            const auto& u = static_cast<const Unary&>(e);
            return ViewNode{std::string("Unary(") + unaryOpSymbol(u.op) + ")", ViewCategory::Expr,
                            e.loc, {viewOf(*u.operand)}};
        }
        case ExprKind::Binary: {
            const auto& b = static_cast<const Binary&>(e);
            return ViewNode{std::string("Binary(") + binaryOpSymbol(b.op) + ")",
                            ViewCategory::Expr, e.loc, {viewOf(*b.left), viewOf(*b.right)}};
        }
        case ExprKind::IsNull: {
            const auto& n = static_cast<const IsNull&>(e);
            return ViewNode{n.negated ? "IsNotNull" : "IsNull", ViewCategory::Expr, e.loc,
                            {viewOf(*n.operand)}};
        }
        case ExprKind::InList: {
            const auto& in = static_cast<const InList&>(e);
            std::vector<ViewNode> items;
            for (const auto& item : in.items) items.push_back(viewOf(*item));
            return ViewNode{in.negated ? "NotInList" : "InList", ViewCategory::Expr, e.loc,
                            {viewOf(*in.operand), clause("Lista", e.loc, std::move(items))}};
        }
        case ExprKind::InSubquery: {
            const auto& in = static_cast<const InSubquery&>(e);
            return ViewNode{in.negated ? "NotInSubquery" : "InSubquery", ViewCategory::Expr,
                            e.loc, {viewOf(*in.operand), viewOf(*in.subquery)}};
        }
        case ExprKind::Exists: {
            const auto& ex = static_cast<const Exists&>(e);
            return ViewNode{ex.negated ? "NotExists" : "Exists", ViewCategory::Expr, e.loc,
                            {viewOf(*ex.subquery)}};
        }
        case ExprKind::ScalarSubquery: {
            const auto& sq = static_cast<const ScalarSubquery&>(e);
            return ViewNode{"ScalarSubquery", ViewCategory::Expr, e.loc, {viewOf(*sq.subquery)}};
        }
        case ExprKind::Aggregate: {
            const auto& agg = static_cast<const Aggregate&>(e);
            std::string label = std::string("Aggregate(") + aggFuncName(agg.func) +
                                (agg.star ? " *)" : ")");
            ViewNode node{label, ViewCategory::Expr, e.loc, {}};
            if (agg.arg) node.children.push_back(viewOf(*agg.arg));
            return node;
        }
    }
    return leaf("?", ViewCategory::Expr, e.loc);
}

ViewNode viewOf(const SelectItem& item) {
    switch (item.kind) {
        case SelectItemKind::AllColumns:
            return leaf("*", ViewCategory::Column, item.loc);
        case SelectItemKind::TableColumns:
            return leaf(item.table.name + ".*", ViewCategory::Column, item.loc);
        case SelectItemKind::Expr:
            if (item.alias.empty()) return viewOf(*item.expr);
            return ViewNode{"AS " + item.alias.name, ViewCategory::Clause, item.loc,
                            {viewOf(*item.expr)}};
    }
    return leaf("?", ViewCategory::Column, item.loc);
}

ViewNode viewOf(const Select& select) {
    ViewNode node{"Select", ViewCategory::Statement, select.loc, {}};

    std::vector<ViewNode> items;
    for (const auto& item : select.items) items.push_back(viewOf(item));
    node.children.push_back(clause("Columnas", select.loc, std::move(items)));

    node.children.push_back(
        leaf("FROM " + tableRefText(select.from), ViewCategory::Clause, select.from.loc));
    for (const auto& join : select.joins) {
        node.children.push_back(clause("JOIN " + tableRefText(join.table), join.loc,
                                       {clause("ON", join.on->loc, {viewOf(*join.on)})}));
    }
    if (select.where) {
        node.children.push_back(clause("WHERE", select.where->loc, {viewOf(*select.where)}));
    }
    if (!select.groupBy.empty()) {
        std::vector<ViewNode> keys;
        for (const auto& key : select.groupBy) keys.push_back(viewOf(*key));
        node.children.push_back(clause("GROUP BY", select.groupBy[0]->loc, std::move(keys)));
    }
    if (select.having) {
        node.children.push_back(clause("HAVING", select.having->loc, {viewOf(*select.having)}));
    }
    if (!select.orderBy.empty()) {
        std::vector<ViewNode> keys;
        for (const auto& item : select.orderBy) {
            keys.push_back(clause(item.descending ? "DESC" : "ASC", item.loc,
                                  {viewOf(*item.expr)}));
        }
        node.children.push_back(clause("ORDER BY", select.orderBy[0].loc, std::move(keys)));
    }
    return node;
}

ViewNode viewOf(const Statement& stmt) {
    switch (stmt.kind) {
        case StatementKind::CreateTable: {
            const auto& s = static_cast<const CreateTable&>(stmt);
            ViewNode node{"CreateTable " + s.table.name, ViewCategory::Statement, s.loc, {}};
            for (const auto& col : s.columns) {
                std::string label = col.name.name + " " + typeSpecText(col.type);
                if (col.notNull) label += " NOT NULL";
                if (col.primaryKey) label += " PRIMARY KEY";
                node.children.push_back(leaf(label, ViewCategory::Definition, col.loc));
            }
            for (const auto& pk : s.primaryKeyClauses) {
                node.children.push_back(
                    leaf("PRIMARY KEY (" + pk.name + ")", ViewCategory::Definition, pk.loc));
            }
            return node;
        }
        case StatementKind::DropTable: {
            const auto& s = static_cast<const DropTable&>(stmt);
            return leaf("DropTable " + s.table.name, ViewCategory::Statement, s.loc);
        }
        case StatementKind::CreateIndex: {
            const auto& s = static_cast<const CreateIndex&>(stmt);
            return leaf("CreateIndex " + s.index.name + " ON " + s.table.name + " (" +
                            s.column.name + ")",
                        ViewCategory::Statement, s.loc);
        }
        case StatementKind::DropIndex: {
            const auto& s = static_cast<const DropIndex&>(stmt);
            return leaf("DropIndex " + s.index.name, ViewCategory::Statement, s.loc);
        }
        case StatementKind::Insert: {
            const auto& s = static_cast<const Insert&>(stmt);
            ViewNode node{"Insert " + s.table.name, ViewCategory::Statement, s.loc, {}};
            if (s.explicitColumns) {
                std::vector<ViewNode> cols;
                for (const auto& c : s.columns) cols.push_back(leaf(c.name, ViewCategory::Column, c.loc));
                node.children.push_back(clause("Columnas", s.loc, std::move(cols)));
            }
            for (size_t i = 0; i < s.rows.size(); ++i) {
                std::vector<ViewNode> values;
                for (const auto& v : s.rows[i]) values.push_back(viewOf(*v));
                node.children.push_back(
                    clause("Fila " + std::to_string(i + 1), s.rowLocs[i], std::move(values)));
            }
            return node;
        }
        case StatementKind::Update: {
            const auto& s = static_cast<const Update&>(stmt);
            ViewNode node{"Update " + s.table.name, ViewCategory::Statement, s.loc, {}};
            for (const auto& a : s.assignments) {
                node.children.push_back(clause("SET " + a.column.name, a.column.loc, {viewOf(*a.value)}));
            }
            if (s.where) node.children.push_back(clause("WHERE", s.where->loc, {viewOf(*s.where)}));
            return node;
        }
        case StatementKind::Delete: {
            const auto& s = static_cast<const Delete&>(stmt);
            ViewNode node{"Delete " + s.table.name, ViewCategory::Statement, s.loc, {}};
            if (s.where) node.children.push_back(clause("WHERE", s.where->loc, {viewOf(*s.where)}));
            return node;
        }
        case StatementKind::Select:
            return viewOf(*static_cast<const SelectStatement&>(stmt).query);
    }
    return leaf("?", ViewCategory::Statement, stmt.loc);
}

}  // namespace

ViewNode viewOf(const Script& script) {
    ViewNode root{"Script", ViewCategory::Script, {1, 1}, {}};
    for (const auto& stmt : script.statements) root.children.push_back(viewOf(*stmt));
    return root;
}

}  // namespace ast
}  // namespace sqlint
