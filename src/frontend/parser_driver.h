#ifndef COMPISCRIPT_FRONTEND_PARSER_DRIVER_H
#define COMPISCRIPT_FRONTEND_PARSER_DRIVER_H

// Unico punto de entrada al frontend. No expone ningun tipo de ANTLR: el
// resto del compilador (compiler/, ast/, diagnostics/) puede incluir este
// header sin arrastrar ANTLR con el. Esa es la regla verificable del
// diseno: `grep -r antlr src/ --exclude-dir=frontend` no debe encontrar nada.

#include <string>

#include "ast/nodes.h"
#include "diagnostics/reporter.h"

namespace compiscript {
namespace frontend {

// Corre lexer + parser (ANTLR) + AstBuilder sobre `source`. Los errores
// sinacticos se acumulan en `reporter` (via DiagnosticErrorListener) en vez
// de salir por stderr. Retorna null si el parser no pudo construir un
// Program valido.
ast::ProgramPtr parse(const std::string& source, diagnostics::DiagnosticReporter& reporter);

}  // namespace frontend
}  // namespace compiscript

#endif  // COMPISCRIPT_FRONTEND_PARSER_DRIVER_H
