#include "script_checker.h"

#include "analyzer.h"
#include "type_rules.h"

namespace sqlint {
namespace semantic {

storage::TableDef toTableDef(const ast::CreateTable& stmt) {
    storage::TableDef table;
    table.name = stmt.table.name;
    for (const auto& col : stmt.columns) {
        storage::ColumnDef def;
        def.name = col.name.name;
        def.type = fromSpec(col.type);
        def.primaryKey = col.primaryKey;
        for (const auto& pk : stmt.primaryKeyClauses) {
            if (storage::sameName(pk.name, col.name.name)) def.primaryKey = true;
        }
        def.notNull = col.notNull || def.primaryKey;
        table.columns.push_back(def);
    }
    return table;
}

void simulateSchema(const ast::Statement& stmt, storage::Catalog& catalog) {
    switch (stmt.kind) {
        case ast::StatementKind::CreateTable:
            catalog.addTable(toTableDef(static_cast<const ast::CreateTable&>(stmt))); break;
        case ast::StatementKind::DropTable:
            catalog.dropTable(static_cast<const ast::DropTable&>(stmt).table.name); break;
        case ast::StatementKind::CreateIndex: {
            const auto& s = static_cast<const ast::CreateIndex&>(stmt);
            catalog.addIndex({s.index.name, s.table.name, s.column.name, "indices/" + s.index.name + ".idx"});
            break;
        }
        case ast::StatementKind::DropIndex:
            catalog.dropIndex(static_cast<const ast::DropIndex&>(stmt).index.name); break;
        default: break;
    }
}

ScriptCheck checkScript(ast::Script& script, storage::Catalog catalog) {
    ScriptCheck result;
    for (auto& stmt : script.statements) {
        std::vector<diagnostics::Diagnostic> diags = analyze(*stmt, catalog);
        if (!diags.empty()) {
            result.diagnostics = std::move(diags);
            return result;
        }
        ++result.statementsOk;

        simulateSchema(*stmt, catalog);
    }
    return result;
}

}  // namespace semantic
}  // namespace sqlint
