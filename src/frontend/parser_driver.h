#ifndef SQLINT_FRONTEND_PARSER_DRIVER_H
#define SQLINT_FRONTEND_PARSER_DRIVER_H

// Unico punto de entrada al frontend. No expone tipos de ANTLR: main.cpp y
// las Partes 2 y 3 incluyen este header sin arrastrar ANTLR. Regla
// verificable: `grep -rl antlr src/ --exclude-dir=frontend` no encuentra nada.

#include <string>
#include <vector>

#include "diagnostics/reporter.h"
#include "token_dump.h"

namespace sqlint {
namespace frontend {

struct ParseOptions {
    bool wantParseTreeDot = false;  // --emit parse-tree
};

struct ParseOutput {
    // Tokens del canal por defecto mas los tokens de error (ERROR_CHAR,
    // UNTERMINATED_STRING), en orden. Sin espacios, comentarios ni EOF.
    std::vector<TokenInfo> tokens;
    // Solo si ParseOptions::wantParseTreeDot. Se genera aunque haya errores:
    // el arbol parcial que arma ANTLR al recuperarse tambien sirve.
    std::string parseTreeDot;
};

// Corre lexer y parser sobre `source`. Los errores lexicos (LEX001, LEX002)
// y sintacticos (SYN001) se acumulan en `reporter`; si hay alguno, el script
// no se debe ejecutar.
ParseOutput parse(const std::string& source, diagnostics::DiagnosticReporter& reporter,
                  const ParseOptions& options = {});

}  // namespace frontend
}  // namespace sqlint

#endif  // SQLINT_FRONTEND_PARSER_DRIVER_H
