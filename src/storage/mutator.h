#ifndef SQLINT_STORAGE_MUTATOR_H
#define SQLINT_STORAGE_MUTATOR_H

// Ejecucion de las sentencias que modifican la base (docs/sql/parte2 5.4 y
// 6.1-6.3; dueno: Parte 2): CREATE/DROP TABLE, CREATE/DROP INDEX, INSERT,
// UPDATE y DELETE.
//
// Recibe el AST ya validado por el semantico: no repite sus revisiones de
// nombres ni de tipos, solo las que dependen de los datos (RUN101 llave
// repetida, RUN102 NOT NULL, RUN104 largo de VARCHAR). Todas se revisan
// ANTES de escribir: una sentencia que falla no deja la base a medias.
//
// Las expresiones y el WHERE los resuelve la Parte 3 a traves de ExecHooks:
//   eval          valor de una expresion (VALUES, SET)
//   matchingRows  filas de UPDATE/DELETE; el planificador elige indice o
//                 recorrido, asi que UPDATE y DELETE aprovechan los indices
// Mientras la Parte 3 no los tenga, fallbackHooks() (fallback_exec.h) da
// una version minima basada en scan.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "ast/nodes.h"
#include "catalog.h"
#include "index/index_manager.h"
#include "table_store.h"

namespace sqlint {
namespace storage {

struct ExecHooks {
    // `row` es la fila actual de la tabla de la sentencia (para el SET de
    // UPDATE) o nullptr (VALUES de INSERT).
    std::function<Value(const ast::Expr&, const TableDef&, const Row* row)> eval;
    // Filas de la tabla que cumplen el WHERE (where == nullptr: todas).
    std::function<std::vector<StoredRow>(const TableDef&, const ast::Expr* where)> matchingRows;
};

struct MutationResult {
    int64_t affected = 0;  // filas insertadas/actualizadas/borradas; 0 en DDL
    std::string message;   // "3 filas insertadas", "Tabla 'x' creada", ...
};

class Mutator {
public:
    Mutator(Catalog& catalog, TableStore& store, index::IndexManager& indexes, ExecHooks hooks)
        : catalog_(catalog), store_(store), indexes_(indexes), hooks_(std::move(hooks)) {}

    // Ejecuta una sentencia que modifica. Lanza SqlError si falla; en ese
    // caso no cambio ningun archivo. SELECT no es de aqui (Parte 3).
    MutationResult execute(const ast::Statement& stmt);

private:
    MutationResult createTable(const ast::CreateTable& stmt);
    MutationResult dropTable(const ast::DropTable& stmt);
    MutationResult createIndex(const ast::CreateIndex& stmt);
    MutationResult dropIndex(const ast::DropIndex& stmt);
    MutationResult insert(const ast::Insert& stmt);
    MutationResult update(const ast::Update& stmt);
    MutationResult deleteRows(const ast::Delete& stmt);

    Catalog& catalog_;
    TableStore& store_;
    index::IndexManager& indexes_;
    ExecHooks hooks_;
};

// Lleva un valor calculado al tipo de la columna (INT -> FLOAT, cadena ->
// DATE) y revisa el largo de VARCHAR (RUN104). NULL pasa tal cual: el NOT
// NULL lo revisa quien llama, que sabe si es RUN102.
Value coerceToColumn(const Value& value, const ColumnDef& column, diagnostics::SourceLoc loc);

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_MUTATOR_H
