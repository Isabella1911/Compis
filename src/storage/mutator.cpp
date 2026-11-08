#include "mutator.h"

#include <set>

#include "diagnostics/codes.h"
#include "diagnostics/diagnostic.h"
#include "semantic/script_checker.h"

namespace sqlint {
namespace storage {

namespace codes = diagnostics::codes;
using diagnostics::SourceLoc;

namespace {

std::string quoted(const std::string& s) { return "'" + s + "'"; }

[[noreturn]] void fail(const char* code, SourceLoc loc, const std::string& message) {
    diagnostics::Diagnostic d;
    d.phase = diagnostics::Phase::Ejecucion;
    d.code = code;
    d.loc = loc;
    d.message = message;
    throw diagnostics::SqlError(d);
}

std::string rowsMessage(int64_t n, const char* verb) {
    return std::to_string(n) + (n == 1 ? " fila " : " filas ") + verb + (n == 1 ? "" : "s");
}

void checkNotNull(const Row& row, const TableDef& table, SourceLoc loc) {
    for (size_t c = 0; c < table.columns.size(); ++c) {
        if (table.columns[c].notNull && row[c].isNull()) {
            fail(codes::RUN102, loc,
                 "La columna " + quoted(table.columns[c].name) + " de " + quoted(table.name) +
                     " es NOT NULL y el valor calculado es NULL");
        }
    }
}

void duplicateKey(const TableDef& table, int pk, const Value& v, SourceLoc loc) {
    fail(codes::RUN101, loc,
         "Ya existe una fila con " + table.columns[pk].name + " = " + displayValue(v) + " en " +
             quoted(table.name) + " (llave primaria)");
}

std::vector<StoredRow> zipRows(const std::vector<uint64_t>& offsets, std::vector<Row> rows) {
    std::vector<StoredRow> out;
    out.reserve(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) out.push_back(StoredRow{offsets[i], std::move(rows[i])});
    return out;
}

bool valueInSet(const Value& v, const std::set<Value, ValueLess>& keys) {
    return !v.isNull() && keys.count(v) > 0;
}

}  // namespace

Value coerceToColumn(const Value& value, const ColumnDef& column, SourceLoc loc) {
    if (value.isNull()) return value;
    const SqlType& type = column.type;
    if (type.kind == TypeKind::Float && value.kind == TypeKind::Int) {
        return Value::ofFloat(static_cast<double>(value.asInt()));
    }
    if (type.kind == TypeKind::Date && value.kind == TypeKind::Varchar) {
        Date d;
        if (!parseDate(value.asString(), d)) {
            fail(codes::SEM011, loc,
                 "'" + value.asString() + "' no es una fecha válida para la columna " +
                     quoted(column.name));
        }
        return Value::ofDate(d);
    }
    if (type.kind != value.kind) {
        fail(codes::SEM009, loc,
             "La columna " + quoted(column.name) + " es " + typeName(type) +
                 " y el valor calculado es " + displayValue(value));
    }
    if (type.kind == TypeKind::Varchar) {
        int len = utf8Length(value.asString());
        if (len > type.length) {
            fail(codes::RUN104, loc,
                 "El texto tiene " + std::to_string(len) + " caracteres y la columna " +
                     quoted(column.name) + " es " + typeName(type));
        }
    }
    return value;
}

MutationResult Mutator::execute(const ast::Statement& stmt) {
    switch (stmt.kind) {
        case ast::StatementKind::CreateTable:
            return createTable(static_cast<const ast::CreateTable&>(stmt));
        case ast::StatementKind::DropTable:
            return dropTable(static_cast<const ast::DropTable&>(stmt));
        case ast::StatementKind::CreateIndex:
            return createIndex(static_cast<const ast::CreateIndex&>(stmt));
        case ast::StatementKind::DropIndex:
            return dropIndex(static_cast<const ast::DropIndex&>(stmt));
        case ast::StatementKind::Insert: return insert(static_cast<const ast::Insert&>(stmt));
        case ast::StatementKind::Update: return update(static_cast<const ast::Update&>(stmt));
        case ast::StatementKind::Delete: return deleteRows(static_cast<const ast::Delete&>(stmt));
        case ast::StatementKind::Select: break;
    }
    fail(codes::SEM023, stmt.loc, "SELECT no lo ejecuta el Mutator");
}

// ---------------------------------------------------------------------------
// Llaves foraneas
// ---------------------------------------------------------------------------

void Mutator::checkForeignKeys(const TableDef& table, const Row& row, SourceLoc loc) {
    for (const ForeignKeyDef* fk : catalog_.foreignKeysOfTable(table.name)) {
        int c = table.findColumn(fk->column);
        if (c < 0 || row[c].isNull()) continue;
        const TableDef* parent = catalog_.findTable(fk->refTable);
        if (parent == nullptr) continue;
        int ref = parent->findColumn(fk->refColumn);
        if (ref < 0) continue;
        bool found = false;
        for (const auto& parentRow : store_.scan(*parent)) {
            if (!parentRow.values[ref].isNull() &&
                compareValues(parentRow.values[ref], row[c]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            fail(codes::RUN105, loc,
                 "La columna " + quoted(fk->column) + " = " + displayValue(row[c]) +
                     " no existe en " + quoted(fk->refTable) + "." + fk->refColumn +
                     " (llave foránea)");
        }
    }
}

void Mutator::rewriteTable(const TableDef& table, std::vector<Row> rows) {
    std::vector<uint64_t> offsets = store_.rewrite(table, rows);
    indexes_.rebuild(table, zipRows(offsets, std::move(rows)));
}

// Borra en cascada las filas hijas que apuntan a `deletedKeys` en la columna
// referenciada de `parentTable`. Recursivo. Reescribe los CSV e indices hijos.
void Mutator::cascadeDeleteChildren(const std::string& parentTable,
                                    const std::string& refColumn,
                                    const std::set<Value, ValueLess>& deletedKeys,
                                    SourceLoc loc, bool forceCascade) {
    if (deletedKeys.empty()) return;

    // Copia: cascade puede mutar el catalogo solo al hacer DROP, no aqui.
    std::vector<ForeignKeyDef> incoming;
    for (const ForeignKeyDef* fk : catalog_.foreignKeysTo(parentTable)) {
        if (sameName(fk->refColumn, refColumn)) incoming.push_back(*fk);
    }

    for (const ForeignKeyDef& fk : incoming) {
        const TableDef* childPtr = catalog_.findTable(fk.table);
        if (childPtr == nullptr) continue;
        TableDef child = *childPtr;
        int childCol = child.findColumn(fk.column);
        if (childCol < 0) continue;

        std::vector<StoredRow> all = store_.scan(child);
        std::vector<Row> kept;
        std::vector<Row> removed;
        kept.reserve(all.size());
        for (auto& stored : all) {
            if (valueInSet(stored.values[childCol], deletedKeys)) {
                removed.push_back(std::move(stored.values));
            } else {
                kept.push_back(std::move(stored.values));
            }
        }
        if (removed.empty()) continue;

        if (!forceCascade && !fk.onDeleteCascade) {
            fail(codes::RUN106, loc,
                 "No se puede borrar de " + quoted(parentTable) + ": hay filas en " +
                     quoted(fk.table) + " que la referencian por " + quoted(fk.column) +
                     ". Declará ON DELETE CASCADE o borrá las filas hijas antes");
        }

        // Primero cascada desde la hija (si ella es padre de otras), luego
        // reescribir su CSV.
        int childPk = child.primaryKeyColumn();
        if (childPk >= 0) {
            std::set<Value, ValueLess> childKeys;
            for (const auto& row : removed) {
                if (!row[childPk].isNull()) childKeys.insert(row[childPk]);
            }
            cascadeDeleteChildren(child.name, child.columns[childPk].name, childKeys, loc,
                                  forceCascade);
        }
        rewriteTable(child, std::move(kept));
    }
}

// ---------------------------------------------------------------------------
// DDL
// ---------------------------------------------------------------------------

MutationResult Mutator::createTable(const ast::CreateTable& stmt) {
    TableDef def = semantic::toTableDef(stmt);
    // CSV primero, catalogo despues: un crash entre los dos deja un CSV
    // huerfano, que el siguiente CREATE con el mismo nombre sobrescribe.
    store_.create(def);
    catalog_.addTable(def);
    for (const auto& fk : stmt.foreignKeys) {
        catalog_.addForeignKey({def.name, fk.column.name, fk.refTable.name, fk.refColumn.name,
                                fk.onDeleteCascade});
    }
    catalog_.save();
    return {0, "Tabla " + quoted(def.name) + " creada"};
}

MutationResult Mutator::dropTable(const ast::DropTable& stmt) {
    TableDef def = *catalog_.findTable(stmt.table.name);
    auto incoming = catalog_.foreignKeysTo(def.name);
    if (!stmt.cascade && !incoming.empty()) {
        fail(codes::SEM024, stmt.loc,
             "No se puede hacer DROP TABLE de " + quoted(def.name) +
                 " porque otras tablas la referencian. Usá CASCADE");
    }

    // CASCADE: borrar primero las filas hijas (reescribe sus CSV) y luego
    // la tabla padre. forceCascade=true: DROP CASCADE limpia datos hijos
    // aunque la FK no declare ON DELETE CASCADE.
    if (stmt.cascade && !incoming.empty()) {
        std::set<std::string> done;
        for (const ForeignKeyDef* fk : incoming) {
            std::string key = fk->refColumn;
            bool already = false;
            for (const auto& d : done) {
                if (sameName(d, key)) already = true;
            }
            if (already) continue;
            done.insert(fk->refColumn);
            int refCol = def.findColumn(fk->refColumn);
            if (refCol < 0) continue;
            std::set<Value, ValueLess> keys;
            for (const auto& row : store_.scan(def)) {
                if (!row.values[refCol].isNull()) keys.insert(row.values[refCol]);
            }
            cascadeDeleteChildren(def.name, fk->refColumn, keys, stmt.loc, true);
        }
    }

    std::vector<IndexDef> idxs;
    for (const IndexDef* i : catalog_.indexesOfTable(def.name)) idxs.push_back(*i);

    // Catalogo primero: si el borrado de archivos falla, la tabla ya no
    // existe para el interprete y solo quedan archivos huerfanos.
    catalog_.dropTable(def.name);  // tambien saca FKs que la involucran
    catalog_.save();
    store_.drop(def);
    indexes_.removeFiles(idxs);
    return {0, "Tabla " + quoted(def.name) + " eliminada"};
}

MutationResult Mutator::createIndex(const ast::CreateIndex& stmt) {
    indexes_.create(stmt.index.name, stmt.table.name, stmt.column.name);
    return {0, "Índice " + quoted(stmt.index.name) + " creado"};
}

MutationResult Mutator::dropIndex(const ast::DropIndex& stmt) {
    std::string name = catalog_.findIndex(stmt.index.name)->name;
    indexes_.drop(name);
    return {0, "Índice " + quoted(name) + " eliminado"};
}

// ---------------------------------------------------------------------------
// DML
// ---------------------------------------------------------------------------

MutationResult Mutator::insert(const ast::Insert& stmt) {
    const TableDef table = *catalog_.findTable(stmt.table.name);
    std::vector<int> targets;
    for (const auto& name : stmt.columns) targets.push_back(table.findColumn(name.name));

    int pk = table.primaryKeyColumn();
    std::set<Value, ValueLess> keys;
    if (pk >= 0) {
        for (const auto& row : store_.scan(table)) {
            if (!row.values[pk].isNull()) keys.insert(row.values[pk]);
        }
    }

    std::vector<Row> rows;
    for (size_t r = 0; r < stmt.rows.size(); ++r) {
        SourceLoc rowLoc = r < stmt.rowLocs.size() ? stmt.rowLocs[r] : stmt.loc;
        Row row(table.columns.size(), Value::null());
        SourceLoc pkLoc = rowLoc;
        for (size_t i = 0; i < stmt.rows[r].size(); ++i) {
            const ast::Expr& e = *stmt.rows[r][i];
            int c = targets[i];
            row[c] = coerceToColumn(hooks_.eval(e, table, nullptr), table.columns[c], e.loc);
            if (c == pk) pkLoc = e.loc;
        }
        checkNotNull(row, table, rowLoc);
        if (pk >= 0 && !keys.insert(row[pk]).second) duplicateKey(table, pk, row[pk], pkLoc);
        checkForeignKeys(table, row, rowLoc);
        rows.push_back(std::move(row));
    }

    std::vector<StoredRow> added;
    for (auto& row : rows) {
        uint64_t offset = store_.append(table, row);
        added.push_back(StoredRow{offset, std::move(row)});
    }
    indexes_.onInsert(table, added);

    int64_t n = static_cast<int64_t>(added.size());
    return {n, rowsMessage(n, "insertada")};
}

MutationResult Mutator::update(const ast::Update& stmt) {
    const TableDef table = *catalog_.findTable(stmt.table.name);
    std::vector<StoredRow> all = store_.scan(table);
    std::vector<StoredRow> matched = hooks_.matchingRows(table, stmt.where.get());
    std::set<uint64_t> hit;
    for (const auto& row : matched) hit.insert(row.offset);

    std::vector<Row> rows;
    rows.reserve(all.size());
    for (const auto& stored : all) {
        if (!hit.count(stored.offset)) {
            rows.push_back(stored.values);
            continue;
        }
        Row row = stored.values;
        for (const auto& a : stmt.assignments) {
            int c = table.findColumn(a.column.name);
            row[c] = coerceToColumn(hooks_.eval(*a.value, table, &stored.values), table.columns[c],
                                    a.value->loc);
            if (table.columns[c].notNull && row[c].isNull()) {
                fail(codes::RUN102, a.value->loc,
                     "La columna " + quoted(table.columns[c].name) + " de " +
                         quoted(table.name) + " es NOT NULL y el valor calculado es NULL");
            }
        }
        checkForeignKeys(table, row, stmt.loc);
        rows.push_back(std::move(row));
    }

    int pk = table.primaryKeyColumn();
    if (pk >= 0 && !hit.empty()) {
        SourceLoc loc = stmt.loc;
        for (const auto& a : stmt.assignments) {
            if (table.findColumn(a.column.name) == pk) loc = a.value->loc;
        }
        std::set<Value, ValueLess> keys;
        for (const auto& row : rows) {
            if (!keys.insert(row[pk]).second) duplicateKey(table, pk, row[pk], loc);
        }
    }

    int64_t n = static_cast<int64_t>(hit.size());
    if (n == 0) return {0, rowsMessage(0, "actualizada")};

    rewriteTable(table, std::move(rows));
    return {n, rowsMessage(n, "actualizada")};
}

MutationResult Mutator::deleteRows(const ast::Delete& stmt) {
    const TableDef table = *catalog_.findTable(stmt.table.name);
    std::vector<StoredRow> all = store_.scan(table);
    std::vector<StoredRow> matched = hooks_.matchingRows(table, stmt.where.get());
    std::set<uint64_t> hit;
    for (const auto& row : matched) hit.insert(row.offset);

    int64_t n = static_cast<int64_t>(hit.size());
    if (n == 0) return {0, rowsMessage(0, "eliminada")};

    std::vector<Row> kept;
    std::vector<Row> removed;
    for (auto& stored : all) {
        if (hit.count(stored.offset)) {
            removed.push_back(std::move(stored.values));
        } else {
            kept.push_back(std::move(stored.values));
        }
    }

    // Cascada por cada columna del padre referenciada por alguna FK.
    std::vector<std::string> refColsDone;
    for (const ForeignKeyDef* fk : catalog_.foreignKeysTo(table.name)) {
        bool already = false;
        for (const auto& d : refColsDone) {
            if (sameName(d, fk->refColumn)) already = true;
        }
        if (already) continue;
        refColsDone.push_back(fk->refColumn);
        int refCol = table.findColumn(fk->refColumn);
        if (refCol < 0) continue;
        std::set<Value, ValueLess> deleted;
        for (const auto& row : removed) {
            if (!row[refCol].isNull()) deleted.insert(row[refCol]);
        }
        cascadeDeleteChildren(table.name, fk->refColumn, deleted, stmt.loc, false);
    }

    rewriteTable(table, std::move(kept));
    return {n, rowsMessage(n, "eliminada")};
}

}  // namespace storage
}  // namespace sqlint
