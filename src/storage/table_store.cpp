#include "table_store.h"

#include <algorithm>
#include <fstream>

#include "csv.h"
#include "diagnostics/codes.h"
#include "diagnostics/diagnostic.h"
#include "fs_util.h"

namespace sqlint {
namespace storage {

namespace {

[[noreturn]] void corrupt(const std::string& path, int line, const std::string& what) {
    diagnostics::Diagnostic d;
    d.phase = diagnostics::Phase::Ejecucion;
    d.code = diagnostics::codes::RUN103;
    d.message = "El archivo '" + path + "'" +
                (line > 0 ? " está corrupto en la línea " + std::to_string(line) : "") + ": " +
                what;
    throw diagnostics::SqlError(d);
}

std::string headerLine(const TableDef& table) {
    std::vector<std::string> names;
    for (const auto& c : table.columns) names.push_back(toCsvField(Value::ofString(c.name)));
    return formatCsvRecord(names);
}

std::ifstream openOrFail(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) corrupt(path, 0, "no existe o no se puede abrir");
    return in;
}

// Un registro vacio en una tabla de varias columnas es una linea en blanco
// (por ejemplo, al final de un CSV editado a mano): se ignora.
bool isBlank(const CsvRecord& rec, const TableDef& table) {
    return table.columns.size() > 1 && rec.fields.size() == 1 && rec.fields[0].empty();
}

Row toRow(const CsvRecord& rec, const TableDef& table, const std::string& path) {
    if (rec.fields.size() != table.columns.size()) {
        corrupt(path, rec.line,
                "la fila tiene " + std::to_string(rec.fields.size()) + " campo(s) y la tabla " +
                    std::to_string(table.columns.size()) + " columna(s)");
    }
    Row row;
    row.reserve(rec.fields.size());
    for (size_t i = 0; i < rec.fields.size(); ++i) {
        try {
            row.push_back(fromCsvField(rec.fields[i], table.columns[i].type));
        } catch (const diagnostics::SqlError& e) {
            corrupt(path, rec.line, "columna '" + table.columns[i].name + "': " + e.diag.message);
        }
    }
    return row;
}

// Lee y valida el encabezado. Deja `reader` en la primera fila.
void readHeader(CsvReader& reader, const TableDef& table, const std::string& path) {
    CsvRecord header;
    try {
        if (!reader.next(header)) corrupt(path, 1, "falta el encabezado");
    } catch (const CsvError& e) {
        corrupt(path, e.line, e.what());
    }
    bool ok = header.fields.size() == table.columns.size();
    for (size_t i = 0; ok && i < header.fields.size(); ++i) {
        Value name = fromCsvField(header.fields[i], SqlType{TypeKind::Varchar, 0});
        ok = !name.isNull() && sameName(name.asString(), table.columns[i].name);
    }
    if (!ok) corrupt(path, 1, "el encabezado no coincide con las columnas de la tabla");
}

}  // namespace

std::string formatRow(const Row& row) {
    std::vector<std::string> fields;
    fields.reserve(row.size());
    for (const auto& v : row) fields.push_back(toCsvField(v));
    return formatCsvRecord(fields);
}

std::string TableStore::pathOf(const TableDef& table) const {
    return joinPath(dbDir_, table.name + ".csv");
}

std::vector<StoredRow> TableStore::scan(const TableDef& table) {
    std::string path = pathOf(table);
    std::ifstream in = openOrFail(path);
    CsvReader reader(in);
    readHeader(reader, table, path);

    std::vector<StoredRow> rows;
    CsvRecord rec;
    try {
        while (reader.next(rec)) {
            if (isBlank(rec, table)) continue;
            rows.push_back(StoredRow{rec.offset, toRow(rec, table, path)});
        }
    } catch (const CsvError& e) {
        corrupt(path, e.line, e.what());
    }
    return rows;
}

std::vector<StoredRow> TableStore::readAt(const TableDef& table,
                                          const std::vector<uint64_t>& offsets) {
    std::vector<uint64_t> sorted = offsets;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    std::string path = pathOf(table);
    std::ifstream in = openOrFail(path);
    std::vector<StoredRow> rows;
    rows.reserve(sorted.size());
    for (uint64_t off : sorted) {
        // Una fila empieza justo despues de un "\n"; si no, el offset no es
        // de una fila (el byte 0 es el encabezado).
        bool startsLine = false;
        if (off > 0) {
            in.clear();
            in.seekg(static_cast<std::streamoff>(off - 1));
            startsLine = in && in.get() == '\n';
        }
        CsvReader reader(in, ',', off, 0);  // linea desconocida: 0
        CsvRecord rec;
        try {
            if (!startsLine || !reader.next(rec)) {
                corrupt(path, 0, "no hay fila en el byte " + std::to_string(off) +
                                     " (índice desactualizado)");
            }
        } catch (const CsvError& e) {
            corrupt(path, 0, std::string(e.what()) + " en el byte " + std::to_string(off));
        }
        rows.push_back(StoredRow{off, toRow(rec, table, path)});
    }
    return rows;
}

uint64_t TableStore::append(const TableDef& table, const Row& row) {
    std::string path = pathOf(table);
    std::string prefix;
    uint64_t size = 0;
    {
        std::ifstream in = openOrFail(path);
        in.seekg(0, std::ios::end);
        size = static_cast<uint64_t>(in.tellg());
        if (size > 0) {
            // Si alguien dejo el archivo sin "\n" final, la fila nueva no
            // puede quedar pegada a la ultima.
            in.seekg(-1, std::ios::end);
            if (in.get() != '\n') prefix = "\n";
        }
    }
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out.is_open()) corrupt(path, 0, "no se puede escribir");
    out << prefix << formatRow(row);
    out.flush();
    if (!out) corrupt(path, 0, "no se pudo agregar la fila");
    return size + prefix.size();
}

std::vector<uint64_t> TableStore::rewrite(const TableDef& table, const std::vector<Row>& rows) {
    std::string content = headerLine(table);
    std::vector<uint64_t> offsets;
    offsets.reserve(rows.size());
    for (const auto& row : rows) {
        offsets.push_back(content.size());
        content += formatRow(row);
    }
    writeFileAtomic(pathOf(table), content);
    return offsets;
}

void TableStore::create(const TableDef& table) {
    ensureDir(dbDir_);
    writeFileAtomic(pathOf(table), headerLine(table));
}

void TableStore::drop(const TableDef& table) { removeFile(pathOf(table)); }

}  // namespace storage
}  // namespace sqlint
