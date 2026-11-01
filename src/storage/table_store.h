#ifndef SQLINT_STORAGE_TABLE_STORE_H
#define SQLINT_STORAGE_TABLE_STORE_H

// Filas de una tabla en su CSV (contrato A.6 de docs/sql; dueno: Parte 2).
//
// Cada fila se identifica por su offset: el byte del CSV donde empieza su
// linea. Es lo que guardan los indices, y con el readAt hace seekg directo a
// esas filas sin leer el resto del archivo.
//
// Los offsets solo cambian con rewrite (UPDATE y DELETE): append deja las
// filas viejas donde estaban. Por eso el indice se actualiza incremental en
// INSERT y se reconstruye despues de UPDATE y DELETE (docs/sql/parte2 6.3).

#include <cstdint>
#include <string>
#include <vector>

#include "catalog.h"
#include "value.h"

namespace sqlint {
namespace storage {

using Row = std::vector<Value>;

struct StoredRow {
    uint64_t offset = 0;  // byte donde empieza la linea en el CSV
    Row values;
};

class TableStore {
public:
    explicit TableStore(std::string dbDir) : dbDir_(std::move(dbDir)) {}

    // Recorrido completo, en el orden del archivo. RUN103 si el CSV no
    // existe, no tiene el encabezado del esquema o tiene una fila invalida.
    std::vector<StoredRow> scan(const TableDef& table);

    // Acceso directo: abre el archivo una vez, ordena los offsets (sin
    // repetidos) y lee solo esas filas. El resultado va en orden de offset.
    std::vector<StoredRow> readAt(const TableDef& table, const std::vector<uint64_t>& offsets);

    // Agrega la fila al final y devuelve su offset.
    uint64_t append(const TableDef& table, const Row& row);

    // Reescribe la tabla completa de forma atomica. Devuelve el offset nuevo
    // de cada fila, en el mismo orden que `rows`.
    std::vector<uint64_t> rewrite(const TableDef& table, const std::vector<Row>& rows);

    // CSV nuevo con solo el encabezado (CREATE TABLE).
    void create(const TableDef& table);

    // Borra el CSV (DROP TABLE).
    void drop(const TableDef& table);

    std::string pathOf(const TableDef& table) const;

private:
    std::string dbDir_;
};

// Serializa la fila como una linea del CSV (con "\n").
std::string formatRow(const Row& row);

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_TABLE_STORE_H
