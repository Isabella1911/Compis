#ifndef SQLINT_STORAGE_CATALOG_H
#define SQLINT_STORAGE_CATALOG_H

// ============================================================================
// STUB DE LA PARTE 1. Este archivo es de la Parte 2 (docs/sql, contrato A.6).
// Catalogo solo en memoria: load() no lee metadata.txt, devuelve un catalogo
// vacio, y no hay save(). Alcanza para que el semantico se pruebe con
// scripts que crean sus tablas antes de usarlas. La Parte 2 lo reemplaza con
// la version que lee y escribe metadata.txt, conservando estas firmas.
// ============================================================================

#include <string>
#include <vector>

#include "value.h"

namespace sqlint {
namespace storage {

struct ColumnDef {
    std::string name;
    SqlType type;
    bool notNull = false;
    bool primaryKey = false;
};

struct TableDef {
    std::string name;
    std::vector<ColumnDef> columns;

    int findColumn(const std::string& name) const;  // -1 si no existe; sin distinguir mayusculas
};

struct IndexDef {
    std::string name;
    std::string table;
    std::string column;
    std::string file;
};

class Catalog {
public:
    static Catalog load(const std::string& dbDir);

    // Busquedas sin distinguir mayusculas. Devuelven nullptr si no existe.
    const TableDef* findTable(const std::string& name) const;
    const IndexDef* findIndex(const std::string& name) const;
    std::vector<const IndexDef*> indexesOn(const std::string& table,
                                           const std::string& column) const;
    std::vector<const TableDef*> tables() const;

    // Mutadores en memoria. La Parte 2 los usa al ejecutar; el semantico los
    // usa en --no-exec para simular el efecto de CREATE/DROP sobre una copia.
    void addTable(TableDef table);
    void dropTable(const std::string& name);  // borra tambien sus indices
    void addIndex(IndexDef index);
    void dropIndex(const std::string& name);

private:
    std::vector<TableDef> tables_;
    std::vector<IndexDef> indexes_;
};

// Comparacion de nombres sin distinguir mayusculas (solo ASCII).
bool sameName(const std::string& a, const std::string& b);

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_CATALOG_H
