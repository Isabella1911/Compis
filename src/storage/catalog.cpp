// STUB DE LA PARTE 1: ver catalog.h. Lo reemplaza la Parte 2.

#include "catalog.h"

#include <algorithm>
#include <cctype>

namespace sqlint {
namespace storage {

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

Catalog Catalog::load(const std::string& /*dbDir*/) { return Catalog(); }

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

std::vector<const TableDef*> Catalog::tables() const {
    std::vector<const TableDef*> out;
    for (const auto& t : tables_) out.push_back(&t);
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

}  // namespace storage
}  // namespace sqlint
