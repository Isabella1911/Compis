#ifndef YAPARPARSER_H
#define YAPARPARSER_H

#include "Grammar.h"
#include "Token.cpp"
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

// Valida que cada token producido por el lexer esté declarado en YAPar.
// Reporta errores a stderr; no lanza excepción.
void validarTokensDeEntrada(
    const std::vector<Token>& tokens,
    const std::vector<TokenPosicion>& posiciones,
    const YaparSpec& spec
);

// Valida la consistencia de IGNORE: que todos sus tokens estén en %token
// y que $ no aparezca en IGNORE.  Lanza excepción si encuentra error.
void validarTokensIgnorados(const YaparSpec& spec);

// Advierte sobre tokens declarados en %token que no aparecen en la entrada.
// No es error fatal: una entrada de prueba puede no cubrir todos los tokens.
void advertirTokensDeclaradosNoUsados(
    const std::vector<Token>& tokens,
    const YaparSpec& spec
);

// Devuelve el subconjunto de terminales que participan en el parsing
// (excluye los tokens en IGNORE, incluye $).
std::set<std::string> terminalesParaParsing(const YaparSpec& spec);

// ── Construcción de gramática ─────────────────────────────────────────────────

Gramatica construirGramatica(const YaparSpec& spec);

#endif