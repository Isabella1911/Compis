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
// DDL
// ---------------------------------------------------------------------------

MutationResult Mutator::createTable(const ast::CreateTable& stmt) {
    TableDef def = semantic::toTableDef(stmt);
    // CSV primero, catalogo despues: un crash entre los dos deja un CSV
    // huerfano, que el siguiente CREATE con el mismo nombre sobrescribe.
    store_.create(def);
    catalog_.addTable(def);
    catalog_.save();
    return {0, "Tabla " + quoted(def.name) + " creada"};
}

MutationResult Mutator::dropTable(const ast::DropTable& stmt) {
    TableDef def = *catalog_.findTable(stmt.table.name);
    std::vector<IndexDef> idxs;
    for (const IndexDef* i : catalog_.indexesOfTable(def.name)) idxs.push_back(*i);

    // Catalogo primero: si el borrado de archivos falla, la tabla ya no
    // existe para el interprete y solo quedan archivos huerfanos.
    catalog_.dropTable(def.name);
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

    // Llaves que ya estan en la tabla; las filas nuevas se van agregando
    // para detectar tambien repetidos dentro del mismo INSERT.
    int pk = table.primaryKeyColumn();
    std::set<Value, ValueLess> keys;
    if (pk >= 0) {
        for (const auto& row : store_.scan(table)) {
            if (!row.values[pk].isNull()) keys.insert(row.values[pk]);
        }
    }

    // 1. Calcular y validar todas las filas. Nada se escribe todavia.
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
        rows.push_back(std::move(row));
    }

    // 2. Escribir y mantener los indices de forma incremental.
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

    // 1. Filas finales. Los SET se evaluan con la fila ORIGINAL: con
    //    "SET a = b, b = a" se intercambian, como en SQL.
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
        rows.push_back(std::move(row));
    }

    // 2. La llave primaria tiene que seguir sin repetidos.
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

    // 3. Escribir. El rewrite cambia los offsets: los indices se reconstruyen.
    std::vector<uint64_t> offsets = store_.rewrite(table, rows);
    indexes_.rebuild(table, zipRows(offsets, std::move(rows)));
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

    std::vector<Row> rows;
    for (auto& stored : all) {
        if (!hit.count(stored.offset)) rows.push_back(std::move(stored.values));
    }
    // DELETE sin WHERE deja la tabla vacia pero con su encabezado.
    std::vector<uint64_t> offsets = store_.rewrite(table, rows);
    indexes_.rebuild(table, zipRows(offsets, std::move(rows)));
    return {n, rowsMessage(n, "eliminada")};
}

}  // namespace storage
}  // namespace sqlint
