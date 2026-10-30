#ifndef SQLINT_STORAGE_FALLBACK_EXEC_H
#define SQLINT_STORAGE_FALLBACK_EXEC_H

// PROVISIONAL de la Parte 2, hasta que la Parte 3 entregue eval() y
// matchingRows() (docs/sql/parte2 5.4, ultimo punto). Cuando existan, la
// Session le pasa al Mutator sus propios ExecHooks y este archivo se borra.
//
// Alcance: lo que aparece en VALUES, SET y el WHERE de UPDATE/DELETE sobre
// una sola tabla. Literales, columnas de la tabla, aritmetica, comparaciones,
// AND/OR/NOT con logica de tres valores, IS [NOT] NULL e [NOT] IN (lista).
// Las subconsultas lanzan SEM023. matchingRows siempre hace recorrido
// completo: la eleccion de indice es del planificador de la Parte 3.

#include "ast/nodes.h"
#include "mutator.h"
#include "table_store.h"

namespace sqlint {
namespace storage {

Value fallbackEval(const ast::Expr& expr, const TableDef& table, const Row* row);

ExecHooks fallbackHooks(TableStore& store);

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_FALLBACK_EXEC_H
