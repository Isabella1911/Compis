#ifndef SQLINT_SEMANTIC_SCRIPT_CHECKER_H
#define SQLINT_SEMANTIC_SCRIPT_CHECKER_H

// Analisis de un script completo SIN ejecutarlo (--no-exec).
//
// Recorre las sentencias en orden sobre una COPIA del catalogo: analiza cada
// una y, si pasa, simula su efecto sobre el esquema (CREATE TABLE agrega la
// tabla, DROP TABLE la quita). Asi "CREATE TABLE t ...; INSERT INTO t ..."
// no da SEM001 aunque nada se haya escrito en disco. Se detiene en la
// primera sentencia con errores, igual que la ejecucion real (anexo A.5).
//
// Mientras no exista la Session de la Parte 3, main.cpp usa esto siempre.
// Despues la Session lo puede llamar para su modo --no-exec.

#include <vector>

#include "ast/nodes.h"
#include "diagnostics/diagnostic.h"
#include "storage/catalog.h"

namespace sqlint {
namespace semantic {

struct ScriptCheck {
    std::vector<diagnostics::Diagnostic> diagnostics;
    int statementsOk = 0;  // sentencias que pasaron antes del primer error
};

ScriptCheck checkScript(ast::Script& script, storage::Catalog catalog);

// Definicion de tabla del catalogo a partir de un CREATE TABLE ya validado.
// Una columna PRIMARY KEY queda tambien NOT NULL. La Parte 2 lo puede usar
// al ejecutar CREATE TABLE.
storage::TableDef toTableDef(const ast::CreateTable& stmt);

}  // namespace semantic
}  // namespace sqlint

#endif  // SQLINT_SEMANTIC_SCRIPT_CHECKER_H
