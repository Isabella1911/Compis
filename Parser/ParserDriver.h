#ifndef PARSERDRIVER_H
#define PARSERDRIVER_H

#include "Token.h"
#include <vector>
#include <stdexcept>

class ParserDriver {
private:
    std::vector<Token> tokens;
    std::vector<TokenPosicion> posiciones;
    size_t actual = 0;

public:
    ParserDriver(const std::vector<Token>& t, const std::vector<TokenPosicion>& p);

    const Token& verActual() const;
    void avanzar();
    bool fin() const;
    TokenPosicion obtenerPosicion() const;
    const std::vector<Token>& obtenerTokens() const;
    size_t posicionActual() const;
};

#endif
