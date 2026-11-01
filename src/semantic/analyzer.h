#ifndef SQLINT_SEMANTIC_ANALYZER_H
#define SQLINT_SEMANTIC_ANALYZER_H

// Analisis semantico de UNA sentencia contra el catalogo actual (contrato
// A.6 de docs/sql). Se llama sentencia por sentencia porque un script puede
// crear una tabla y usarla en la linea siguiente: el catalogo tiene que
// reflejar lo que ya se ejecuto (ver docs/sql, anexo A.5).
//
// Si no devuelve errores, el AST queda anotado (campos "[sem]" de
// ast/nodes.h) y la sentencia es ejecutable sin mas validaciones de nombres
// ni de tipos.
//
// Incluye JOIN, agrupacion, agregados, subconsultas e indices. Anota tambien
// SelectItem::source para t.*, referencias a alias de ORDER BY y correlacion.

#include <vector>

#include "ast/nodes.h"
#include "diagnostics/diagnostic.h"
#include "storage/catalog.h"

namespace sqlint {
namespace semantic {

std::vector<diagnostics::Diagnostic> analyze(ast::Statement& stmt,
                                             const storage::Catalog& catalog);

}  // namespace semantic
}  // namespace sqlint

#endif  // SQLINT_SEMANTIC_ANALYZER_H
