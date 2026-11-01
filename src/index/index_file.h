#ifndef SQLINT_INDEX_INDEX_FILE_H
#define SQLINT_INDEX_INDEX_FILE_H

// Archivo de un indice (docs/sql/parte2, seccion 4.3):
//
//   # INDEX idx_estudiantes_edad ON estudiantes(edad) TYPE INT
//   17<TAB>58
//   19<TAB>94
//   20<TAB>31,140
//
// Una linea por valor distinto, en el orden de compareValues: el valor
// serializado igual que en el CSV (toCsvField) y la lista de offsets de las
// filas del CSV que lo tienen. Los NULL no se indexan.
//
// En memoria es un std::map ordenado: la igualdad es find y los rangos son
// lower_bound/upper_bound.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "storage/catalog.h"
#include "storage/value.h"

namespace sqlint {
namespace index {

using IndexMap = std::map<storage::Value, std::vector<uint64_t>, storage::ValueLess>;

struct IndexData {
    storage::SqlType type;  // tipo de la columna indexada
    IndexMap entries;
};

// Escribe el archivo de forma atomica.
void writeIndexFile(const std::string& path, const storage::IndexDef& def,
                    const IndexData& data);

// Lee el archivo. Lanza SqlError RUN103 si falta o esta corrupto.
IndexData readIndexFile(const std::string& path);

}  // namespace index
}  // namespace sqlint

#endif  // SQLINT_INDEX_INDEX_FILE_H
