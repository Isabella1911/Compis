#ifndef SQLINT_FRONTEND_TOKEN_DUMP_H
#define SQLINT_FRONTEND_TOKEN_DUMP_H

// Vista de tokens (--emit tokens). TokenInfo es una copia plana del token de
// ANTLR: este header no incluye nada de ANTLR, asi main.cpp lo puede usar.

#include <string>
#include <vector>

namespace sqlint {
namespace frontend {

struct TokenInfo {
    std::string type;  // nombre simbolico: SELECT, IDENTIFIER, LPAREN, ERROR_CHAR...
    std::string text;  // lexema tal como esta en el script
    int line = 0;      // 1-based
    int col = 0;       // 1-based
};

// Tabla alineada para la consola.
std::string formatTokens(const std::vector<TokenInfo>& tokens);

// Arreglo JSON: [{"type": ..., "text": ..., "line": ..., "col": ...}, ...]
std::string tokensToJson(const std::vector<TokenInfo>& tokens);

}  // namespace frontend
}  // namespace sqlint

#endif  // SQLINT_FRONTEND_TOKEN_DUMP_H
