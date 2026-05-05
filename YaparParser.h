#ifndef YAPARPARSER_H
#define YAPARPARSER_H

#include "Grammar.h"
#include "Token.cpp"
#include <string>
#include <vector>
#include <set>
#include <map>

struct YaparSpec {
    std::set<std::string> tokensDeclarados;
    std::set<std::string> tokensIgnorados;
    std::vector<Produccion> producciones;
    std::string simboloInicial;
};

std::string eliminarComentariosYapar(const std::string& contenido);
void procesarLineaToken(const std::string& linea, YaparSpec& spec);
void procesarLineaIgnore(const std::string& linea, YaparSpec& spec);
YaparSpec leerYapar(const std::string& ruta);
std::vector<Produccion> parsearProducciones(
    const std::string& contenido,
    const YaparSpec& spec
);

ResultadoLexico filtrarTokensIgnorados(
    const ResultadoLexico& entrada,
    const std::set<std::string>& ignorados
);

Gramatica construirGramatica(const YaparSpec& spec);

#endif