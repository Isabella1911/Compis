#include "catalog.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "common/json.h"
#include "diagnostics/codes.h"
#include "diagnostics/diagnostic.h"
#include "fs_util.h"

namespace sqlint {
namespace storage {

namespace {

const char* kMetadataFile = "metadata.txt";

std::vector<std::string> splitWords(const std::string& line) {
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string w;
    while (in >> w) words.push_back(w);
    return words;
}

bool isWord(const std::string& w, const char* expected) { return sameName(w, expected); }

[[noreturn]] void corrupt(const std::string& path, int line, const std::string& what) {
    diagnostics::Diagnostic d;
    d.phase = diagnostics::Phase::Ejecucion;
    d.code = diagnostics::codes::RUN103;
    d.message = "El catálogo '" + path + "' está corrupto en la línea " + std::to_string(line) +
                ": " + what;
    throw diagnostics::SqlError(d);
}

// COLUMN <nombre> <tipo> [NOT NULL] [PRIMARY KEY]
ColumnDef parseColumn(const std::vector<std::string>& w, const std::string& path, int line) {
    if (w.size() < 3) corrupt(path, line, "se esperaba 'COLUMN <nombre> <tipo>'");
    ColumnDef col;
    col.name = w[1];
    if (!parseTypeName(w[2], col.type)) corrupt(path, line, "tipo desconocido '" + w[2] + "'");
    for (size_t i = 3; i < w.size(); i += 2) {
        if (i + 1 < w.size() && isWord(w[i], "NOT") && isWord(w[i + 1], "NULL")) {
            col.notNull = true;
        } else if (i + 1 < w.size() && isWord(w[i], "PRIMARY") && isWord(w[i + 1], "KEY")) {
            col.primaryKey = true;
            col.notNull = true;
        } else {
            corrupt(path, line, "no se entiende '" + w[i] + "'");
        }
    }
    return col;
}

// INDEX <nombre> ON <tabla> (<columna>) FILE <archivo>
// Se aceptan "(col)" y "( col )" pegados o separados.
IndexDef parseIndex(const std::string& text, const std::string& path, int line) {
    std::string spaced;
    for (char c : text) {
        if (c == '(' || c == ')') {
            spaced += ' ';
            spaced += c;
            spaced += ' ';
        } else {
            spaced += c;
        }
    }
    std::vector<std::string> w = splitWords(spaced);
    if (w.size() != 9 || !isWord(w[2], "ON") || w[4] != "(" || w[6] != ")" ||
        !isWord(w[7], "FILE")) {
        corrupt(path, line, "se esperaba 'INDEX <nombre> ON <tabla> (<columna>) FILE <archivo>'");
    }
    return IndexDef{w[1], w[3], w[5], w[8]};
}

std::string columnLine(const ColumnDef& col) {
    std::string out = "  COLUMN " + col.name + " " + typeName(col.type);
    if (col.notNull) out += " NOT NULL";
    if (col.primaryKey) out += " PRIMARY KEY";
    return out;
}

}  // namespace

bool sameName(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

int TableDef::findColumn(const std::string& name) const {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (sameName(columns[i].name, name)) return static_cast<int>(i);
    }
    return -1;
}

int TableDef::primaryKeyColumn() const {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i].primaryKey) return static_cast<int>(i);
    }
    return -1;
}

Catalog Catalog::load(const std::string& dbDir) {
    Catalog catalog;
    catalog.dbDir_ = dbDir;

    std::string path = joinPath(dbDir, kMetadataFile);
    std::string content;
    if (!readFile(path, content)) return catalog;  // base nueva

    std::istringstream in(content);
    std::string raw;
    int lineNo = 0;
    TableDef* current = nullptr;
    int tableLine = 0;
    while (std::getline(in, raw)) {
        ++lineNo;
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        std::vector<std::string> w = splitWords(raw);
        if (w.empty() || w[0][0] == '#') continue;

        if (current != nullptr) {
            if (isWord(w[0], "COLUMN")) {
                ColumnDef col = parseColumn(w, path, lineNo);
                if (current->findColumn(col.name) >= 0) {
                    corrupt(path, lineNo, "columna repetida '" + col.name + "'");
                }
                current->columns.push_back(col);
            } else if (isWord(w[0], "END") && w.size() == 1) {
                if (current->columns.empty()) {
                    corrupt(path, lineNo, "la tabla '" + current->name + "' no tiene columnas");
                }
                current = nullptr;
            } else {
                corrupt(path, lineNo, "se esperaba COLUMN o END");
            }
            continue;
        }

        if (isWord(w[0], "VERSION")) {
            if (w.size() != 2 || w[1] != "1") corrupt(path, lineNo, "versión no soportada");
        } else if (isWord(w[0], "TABLE")) {
            if (w.size() != 2) corrupt(path, lineNo, "se esperaba 'TABLE <nombre>'");
            if (catalog.findTable(w[1]) != nullptr) {
                corrupt(path, lineNo, "tabla repetida '" + w[1] + "'");
            }
            catalog.tables_.push_back(TableDef{w[1], {}});
            current = &catalog.tables_.back();
            tableLine = lineNo;
        } else if (isWord(w[0], "INDEX")) {
            IndexDef idx = parseIndex(raw, path, lineNo);
            const TableDef* table = catalog.findTable(idx.table);
            if (table == nullptr) corrupt(path, lineNo, "el índice usa una tabla que no existe");
            if (table->findColumn(idx.column) < 0) {
                corrupt(path, lineNo, "el índice usa una columna que no existe");
            }
            if (catalog.findIndex(idx.name) != nullptr) {
                corrupt(path, lineNo, "índice repetido '" + idx.name + "'");
            }
            catalog.indexes_.push_back(idx);
        } else {
            corrupt(path, lineNo, "no se entiende '" + w[0] + "'");
        }
    }
    if (current != nullptr) {
        corrupt(path, tableLine, "a la tabla '" + current->name + "' le falta END");
    }
    return catalog;
}

void Catalog::save() const {
    std::ostringstream out;
    out << "# Generado por sqlint. No editar a mano mientras el intérprete corre.\n";
    out << "VERSION 1\n";
    for (const auto& t : tables_) {
        out << "\nTABLE " << t.name << "\n";
        for (const auto& c : t.columns) out << columnLine(c) << "\n";
        out << "END\n";
    }
    if (!indexes_.empty()) out << "\n";
    for (const auto& i : indexes_) {
        out << "INDEX " << i.name << " ON " << i.table << " (" << i.column << ") FILE " << i.file
            << "\n";
    }
    ensureDir(dbDir_);
    writeFileAtomic(joinPath(dbDir_, kMetadataFile), out.str());
}

std::string Catalog::csvPath(const TableDef& table) const {
    return joinPath(dbDir_, table.name + ".csv");
}

std::string Catalog::indexPath(const IndexDef& index) const { return joinPath(dbDir_, index.file); }

const TableDef* Catalog::findTable(const std::string& name) const {
    for (const auto& t : tables_) {
        if (sameName(t.name, name)) return &t;
    }
    return nullptr;
}

const IndexDef* Catalog::findIndex(const std::string& name) const {
    for (const auto& i : indexes_) {
        if (sameName(i.name, name)) return &i;
    }
    return nullptr;
}

std::vector<const IndexDef*> Catalog::indexesOn(const std::string& table,
                                                const std::string& column) const {
    std::vector<const IndexDef*> out;
    for (const auto& i : indexes_) {
        if (sameName(i.table, table) && sameName(i.column, column)) out.push_back(&i);
    }
    return out;
}

std::vector<const IndexDef*> Catalog::indexesOfTable(const std::string& table) const {
    std::vector<const IndexDef*> out;
    for (const auto& i : indexes_) {
        if (sameName(i.table, table)) out.push_back(&i);
    }
    return out;
}

std::vector<const TableDef*> Catalog::tables() const {
    std::vector<const TableDef*> out;
    for (const auto& t : tables_) out.push_back(&t);
    return out;
}

std::vector<const IndexDef*> Catalog::indexes() const {
    std::vector<const IndexDef*> out;
    for (const auto& i : indexes_) out.push_back(&i);
    return out;
}

void Catalog::addTable(TableDef table) { tables_.push_back(std::move(table)); }

void Catalog::dropTable(const std::string& name) {
    tables_.erase(std::remove_if(tables_.begin(), tables_.end(),
                                 [&](const TableDef& t) { return sameName(t.name, name); }),
                  tables_.end());
    indexes_.erase(std::remove_if(indexes_.begin(), indexes_.end(),
                                  [&](const IndexDef& i) { return sameName(i.table, name); }),
                   indexes_.end());
}

void Catalog::addIndex(IndexDef index) { indexes_.push_back(std::move(index)); }

void Catalog::dropIndex(const std::string& name) {
    indexes_.erase(std::remove_if(indexes_.begin(), indexes_.end(),
                                  [&](const IndexDef& i) { return sameName(i.name, name); }),
                   indexes_.end());
}

std::string Catalog::toJson() const {
    using json::jsonString;
    std::ostringstream out;
    out << "{\"tables\": [";
    for (size_t t = 0; t < tables_.size(); ++t) {
        const TableDef& table = tables_[t];
        if (t > 0) out << ",";
        out << "\n    {\"name\": " << jsonString(table.name) << ", \"columns\": [";
        for (size_t c = 0; c < table.columns.size(); ++c) {
            const ColumnDef& col = table.columns[c];
            if (c > 0) out << ", ";
            out << "{\"name\": " << jsonString(col.name)
                << ", \"type\": " << jsonString(typeName(col.type))
                << ", \"notNull\": " << (col.notNull ? "true" : "false")
                << ", \"primaryKey\": " << (col.primaryKey ? "true" : "false") << "}";
        }
        out << "], \"indexes\": [";
        bool first = true;
        for (const auto& i : indexes_) {
            if (!sameName(i.table, table.name)) continue;
            if (!first) out << ", ";
            first = false;
            out << "{\"name\": " << jsonString(i.name) << ", \"column\": " << jsonString(i.column)
                << "}";
        }
        out << "]}";
    }
    out << (tables_.empty() ? "]}" : "\n  ]}");
    return out.str();
}

}  // namespace storage
}  // namespace sqlint
