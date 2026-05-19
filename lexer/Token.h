#ifndef TOKEN_CPP_INCLUDED
#define TOKEN_CPP_INCLUDED

#include <string>
#include <vector>

struct Token {
    std::string id;
    std::string valor;
};

struct TokenPosicion {
    int linea;
    int columna;
};

struct ResultadoLexico {
    std::vector<Token> tokens;
    std::vector<TokenPosicion> posiciones;
};

#endif
