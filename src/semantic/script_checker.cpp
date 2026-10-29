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

ScriptCheck checkScript(ast::Script& script, storage::Catalog catalog) {
    ScriptCheck result;
    for (auto& stmt : script.statements) {
        std::vector<diagnostics::Diagnostic> diags = analyze(*stmt, catalog);
        if (!diags.empty()) {
            result.diagnostics = std::move(diags);
            return result;
        }
        ++result.statementsOk;

        // Efecto de la sentencia sobre el esquema. Los datos no importan aqui.
        if (stmt->kind == ast::StatementKind::CreateTable) {
            catalog.addTable(toTableDef(static_cast<const ast::CreateTable&>(*stmt)));
        } else if (stmt->kind == ast::StatementKind::DropTable) {
            catalog.dropTable(static_cast<const ast::DropTable&>(*stmt).table.name);
        }
    }
    return result;
}

}  // namespace semantic
}  // namespace sqlint
