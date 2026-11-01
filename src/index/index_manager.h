#ifndef SQLINT_INDEX_INDEX_MANAGER_H
#define SQLINT_INDEX_INDEX_MANAGER_H

// Indices de una base (contrato A.6 de docs/sql; dueno: Parte 2).
//
// La Parte 3 (planificador) solo usa lookupEq y lookupRange: devuelven
// offsets que se pasan a TableStore::readAt. El resto lo usa el Mutator:
//
//   CREATE INDEX  -> create: scan de la tabla, archivo .idx y catalogo
//   DROP INDEX    -> drop
//   INSERT        -> onInsert: incremental, con el offset de append
//   UPDATE/DELETE -> rebuild: el rewrite cambia todos los offsets
//   DROP TABLE    -> removeFiles: borra los .idx de la tabla
//
// Cada indice cargado queda en un cache durante la vida del IndexManager (una
// Session). Toda escritura actualiza el archivo y el cache juntos, asi el
// cache nunca devuelve filas que ya no existen.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "index_file.h"
#include "storage/catalog.h"
#include "storage/table_store.h"

namespace sqlint {
namespace index {

class IndexManager {
public:
    IndexManager(storage::Catalog& catalog, storage::TableStore& store)
        : catalog_(catalog), store_(store) {}

    // Offsets de las filas con columna == value. El valor se convierte al
    // tipo del indice: buscar 18.0 en un indice INT encuentra el 18, y
    // '2024-01-15' en uno DATE encuentra esa fecha. NULL no encuentra nada.
    std::vector<uint64_t> lookupEq(const storage::IndexDef& idx, const storage::Value& value);

    // Offsets de las filas con lo <(=) columna <(=) hi. Un extremo ausente
    // es abierto. Cubre <, <=, >, >= y BETWEEN.
    std::vector<uint64_t> lookupRange(const storage::IndexDef& idx,
                                      const std::optional<storage::Value>& lo, bool loIncl,
                                      const std::optional<storage::Value>& hi, bool hiIncl);

    // CREATE INDEX nombre ON tabla (columna). Guarda el catalogo.
    void create(const std::string& name, const std::string& table, const std::string& column);

    // DROP INDEX nombre. Guarda el catalogo.
    void drop(const std::string& name);

    // Filas recien agregadas con append (offset y valores).
    void onInsert(const storage::TableDef& table, const std::vector<storage::StoredRow>& added);

    // Reconstruye todos los indices de la tabla con sus filas actuales.
    void rebuild(const storage::TableDef& table, const std::vector<storage::StoredRow>& rows);

    // Borra los archivos de estos indices y los saca del cache. Lo usa DROP
    // TABLE despues de guardar el catalogo sin la tabla (Catalog::dropTable
    // ya quito sus indices).
    void removeFiles(const std::vector<storage::IndexDef>& indexes);

private:
    IndexData& load(const storage::IndexDef& idx);
    void store(const storage::IndexDef& idx, IndexData data);
    static IndexData build(const storage::TableDef& table, int column,
                           const std::vector<storage::StoredRow>& rows);

    storage::Catalog& catalog_;
    storage::TableStore& store_;
    std::map<std::string, IndexData> cache_;  // por nombre en minusculas
};

}  // namespace index
}  // namespace sqlint

#endif  // SQLINT_INDEX_INDEX_MANAGER_H
