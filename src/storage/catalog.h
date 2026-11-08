#ifndef SQLINT_STORAGE_CATALOG_H
#define SQLINT_STORAGE_CATALOG_H

// Catalogo de la base: esquemas de tablas, indices y llaves foraneas
// (contrato A.6 de docs/sql; dueno: Parte 2). Vive en <dbDir>/metadata.txt:
//
//   VERSION 1
//   TABLE estudiantes
//     COLUMN id INT NOT NULL PRIMARY KEY
//     COLUMN nombre VARCHAR(50) NOT NULL
//   END
//   TABLE inscripciones
//     COLUMN estudiante_id INT NOT NULL
//     FOREIGN KEY (estudiante_id) REFERENCES estudiantes (id) ON DELETE CASCADE
//   END
//   INDEX idx_estudiantes_edad ON estudiantes (edad) FILE indices/idx_estudiantes_edad.idx
//
// Los nombres se guardan como se escribieron y se comparan sin distinguir
// mayusculas. Una base nueva es solo un directorio nuevo: si no hay
// metadata.txt, load() devuelve un catalogo vacio y el primer save() crea
// el directorio. load() no escribe nada (sirve tambien para --no-exec).

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
    int primaryKeyColumn() const;                   // -1 si la tabla no tiene llave primaria
};

struct IndexDef {
    std::string name;
    std::string table;
    std::string column;
    std::string file;  // relativo al directorio de la base: indices/<nombre>.idx
};

struct ForeignKeyDef {
    std::string table;      // tabla hija
    std::string column;     // columna en la hija
    std::string refTable;   // tabla padre
    std::string refColumn;  // columna referenciada (suele ser PRIMARY KEY)
    bool onDeleteCascade = false;
};

class Catalog {
public:
    // Lee <dbDir>/metadata.txt. Lanza SqlError RUN103 (con la linea) si el
    // archivo esta corrupto.
    static Catalog load(const std::string& dbDir);

    // Reescribe metadata.txt de forma atomica (metadata.txt.tmp + rename).
    void save() const;

    const std::string& dbDir() const { return dbDir_; }
    std::string csvPath(const TableDef& table) const;    // <dbDir>/<tabla>.csv
    std::string indexPath(const IndexDef& index) const;  // <dbDir>/<file>

    // Busquedas sin distinguir mayusculas. Devuelven nullptr si no existe.
    const TableDef* findTable(const std::string& name) const;
    const IndexDef* findIndex(const std::string& name) const;
    std::vector<const IndexDef*> indexesOn(const std::string& table,
                                           const std::string& column) const;
    std::vector<const IndexDef*> indexesOfTable(const std::string& table) const;
    std::vector<const TableDef*> tables() const;
    std::vector<const IndexDef*> indexes() const;
    std::vector<const ForeignKeyDef*> foreignKeys() const;
    std::vector<const ForeignKeyDef*> foreignKeysOfTable(const std::string& table) const;
    std::vector<const ForeignKeyDef*> foreignKeysTo(const std::string& refTable) const;

    // Mutadores en memoria (no guardan: quien los llama decide cuando hacer
    // save()). La Parte 2 los usa al ejecutar; el semantico los usa en
    // --no-exec para simular el efecto de CREATE/DROP sobre una copia.
    void addTable(TableDef table);
    void dropTable(const std::string& name);  // borra tambien sus indices y FKs
    void addIndex(IndexDef index);
    void dropIndex(const std::string& name);
    void addForeignKey(ForeignKeyDef fk);
    void dropForeignKeysInvolving(const std::string& table);

    // Tablas, columnas, tipos, indices y FKs en JSON, para la IDE (--emit catalog).
    std::string toJson() const;

private:
    std::string dbDir_;
    std::vector<TableDef> tables_;
    std::vector<IndexDef> indexes_;
    std::vector<ForeignKeyDef> foreignKeys_;
};

// Comparacion de nombres sin distinguir mayusculas (solo ASCII).
bool sameName(const std::string& a, const std::string& b);

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_CATALOG_H
