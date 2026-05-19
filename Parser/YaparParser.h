#ifndef YAPARPARSER_H
#define YAPARPARSER_H

#include "Grammar.h"
#include "../lexer/Token.h"
#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>

struct YaparSpec {
    std::set<std::string> tokensDeclarados;
    std::set<std::string> tokensIgnorados;
    std::vector<Produccion> producciones;
    std::string simboloInicial;
};

// ── Parsing de archivo YAPar ──────────────────────────────────────────────────

std::string eliminarComentariosYapar(const std::string& contenido);
void procesarLineaToken(const std::string& linea, YaparSpec& spec);
void procesarLineaIgnore(const std::string& linea, YaparSpec& spec);
YaparSpec leerYapar(const std::string& ruta);
std::vector<Produccion> parsearProducciones(
    const std::string& contenido,
    const YaparSpec& spec
);

// ── Filtrado ──────────────────────────────────────────────────────────────────

ResultadoLexico filtrarTokensIgnorados(
    const ResultadoLexico& entrada,
    const std::set<std::string>& ignorados
);

// ── Validaciones (Avance 4) ───────────────────────────────────────────────────

void validarTokensDeEntrada(
    const std::vector<Token>& tokens,
    const std::vector<TokenPosicion>& posiciones,
    const YaparSpec& spec
);

void validarTokensIgnorados(const YaparSpec& spec);

void advertirTokensDeclaradosNoUsados(
    const std::vector<Token>& tokens,
    const YaparSpec& spec
);

std::set<std::string> terminalesParaParsing(const YaparSpec& spec);

// ── Construccion de gramatica ─────────────────────────────────────────────────

Gramatica construirGramatica(const YaparSpec& spec);

#endif