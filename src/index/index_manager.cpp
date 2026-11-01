#include "index_manager.h"

#include <algorithm>
#include <cctype>

#include "storage/fs_util.h"

namespace sqlint {
namespace index {

using storage::Date;
using storage::IndexDef;
using storage::StoredRow;
using storage::TableDef;
using storage::TypeKind;
using storage::Value;

namespace {

std::string key(const std::string& name) {
    std::string out = name;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// El valor buscado, llevado al tipo del indice. nullopt si no puede haber
// coincidencias (NULL, o una cadena que no es fecha en un indice DATE).
std::optional<Value> toIndexType(const Value& v, const storage::SqlType& type) {
    if (v.isNull()) return std::nullopt;
    if (type.kind == TypeKind::Date && v.kind == TypeKind::Varchar) {
        Date d;
        if (!storage::parseDate(v.asString(), d)) return std::nullopt;
        return Value::ofDate(d);
    }
    return v;  // INT contra FLOAT ya lo resuelve compareValues
}

std::vector<uint64_t> collect(IndexMap::const_iterator from, IndexMap::const_iterator to) {
    std::vector<uint64_t> out;
    for (auto it = from; it != to; ++it) out.insert(out.end(), it->second.begin(), it->second.end());
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

IndexData& IndexManager::load(const IndexDef& idx) {
    auto it = cache_.find(key(idx.name));
    if (it != cache_.end()) return it->second;
    return cache_[key(idx.name)] = readIndexFile(catalog_.indexPath(idx));
}

void IndexManager::store(const IndexDef& idx, IndexData data) {
    writeIndexFile(catalog_.indexPath(idx), idx, data);
    cache_[key(idx.name)] = std::move(data);
}

IndexData IndexManager::build(const TableDef& table, int column,
                              const std::vector<StoredRow>& rows) {
    IndexData data;
    data.type = table.columns[column].type;
    for (const auto& row : rows) {
        const Value& v = row.values[column];
        if (!v.isNull()) data.entries[v].push_back(row.offset);
    }
    for (auto& [value, offsets] : data.entries) std::sort(offsets.begin(), offsets.end());
    return data;
}

std::vector<uint64_t> IndexManager::lookupEq(const IndexDef& idx, const Value& value) {
    const IndexData& data = load(idx);
    std::optional<Value> v = toIndexType(value, data.type);
    if (!v) return {};
    auto it = data.entries.find(*v);
    if (it == data.entries.end()) return {};
    return it->second;
}

std::vector<uint64_t> IndexManager::lookupRange(const IndexDef& idx,
                                                const std::optional<Value>& lo, bool loIncl,
                                                const std::optional<Value>& hi, bool hiIncl) {
    const IndexData& data = load(idx);
    std::optional<Value> l, h;
    if (lo && !(l = toIndexType(*lo, data.type))) return {};
    if (hi && !(h = toIndexType(*hi, data.type))) return {};
    // Rango vacio (lo > hi, o lo == hi sin incluir ambos): sin esta revision
    // `from` quedaria despues de `to` y el recorrido se pasaria del final.
    if (l && h) {
        int c = storage::compareValues(*l, *h);
        if (c > 0 || (c == 0 && !(loIncl && hiIncl))) return {};
    }
    auto from = !l ? data.entries.begin()
                   : (loIncl ? data.entries.lower_bound(*l) : data.entries.upper_bound(*l));
    auto to = !h ? data.entries.end()
                 : (hiIncl ? data.entries.upper_bound(*h) : data.entries.lower_bound(*h));
    return collect(from, to);
}

void IndexManager::create(const std::string& name, const std::string& tableName,
                          const std::string& column) {
    const TableDef* table = catalog_.findTable(tableName);
    int col = table->findColumn(column);
    IndexDef def{name, table->name, table->columns[col].name, "indices/" + name + ".idx"};

    // Primero el archivo y despues el catalogo: si algo falla a la mitad
    // queda un .idx huerfano, nunca un catalogo que apunta a un archivo que
    // no existe.
    store(def, build(*table, col, store_.scan(*table)));
    catalog_.addIndex(def);
    catalog_.save();
}

void IndexManager::drop(const std::string& name) {
    IndexDef def = *catalog_.findIndex(name);
    catalog_.dropIndex(name);
    catalog_.save();
    storage::removeFile(catalog_.indexPath(def));
    cache_.erase(key(name));
}

void IndexManager::onInsert(const TableDef& table, const std::vector<StoredRow>& added) {
    for (const IndexDef* idx : catalog_.indexesOfTable(table.name)) {
        int col = table.findColumn(idx->column);
        IndexData data = load(*idx);
        for (const auto& row : added) {
            const Value& v = row.values[col];
            if (v.isNull()) continue;
            auto& offsets = data.entries[v];
            offsets.insert(std::upper_bound(offsets.begin(), offsets.end(), row.offset),
                           row.offset);
        }
        store(*idx, std::move(data));
    }
}

void IndexManager::rebuild(const TableDef& table, const std::vector<StoredRow>& rows) {
    for (const IndexDef* idx : catalog_.indexesOfTable(table.name)) {
        store(*idx, build(table, table.findColumn(idx->column), rows));
    }
}

void IndexManager::removeFiles(const std::vector<IndexDef>& indexes) {
    for (const IndexDef& idx : indexes) {
        storage::removeFile(catalog_.indexPath(idx));
        cache_.erase(key(idx.name));
    }
}

}  // namespace index
}  // namespace sqlint
