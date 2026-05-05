#include "ParserDriver.h"

ParserDriver::ParserDriver(const std::vector<Token>& t, const std::vector<TokenPosicion>& p)
    : tokens(t), posiciones(p), actual(0) {
    if (tokens.size() != posiciones.size()) {
        throw std::runtime_error("Error interno: tokens y posiciones tienen tamaños diferentes");
    }
    if (tokens.empty()) {
        throw std::runtime_error("Error interno: lista de tokens vacía");
    }
}

const Token& ParserDriver::verActual() const {
    if (actual >= tokens.size()) {
        throw std::runtime_error("Error interno: acceso a token fuera de rango");
    }
    return tokens[actual];
}

void ParserDriver::avanzar() {
    if (actual < tokens.size() - 1) {
        actual++;
    }
}

bool ParserDriver::fin() const {
    return actual >= tokens.size() || tokens[actual].id == "$";
}

TokenPosicion ParserDriver::obtenerPosicion() const {
    if (actual >= posiciones.size()) {
        return {-1, -1};
    }
    return posiciones[actual];
}

const std::vector<Token>& ParserDriver::obtenerTokens() const {
    return tokens;
}

size_t ParserDriver::posicionActual() const {
    return actual;
}
