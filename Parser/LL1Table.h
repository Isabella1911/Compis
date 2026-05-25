#ifndef LL1TABLE_H
#define LL1TABLE_H

#include "Grammar.h"
#include "FirstFollow.h"
#include "../lexer/Token.h"
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

using TablaLL1 = std::map<std::string, std::map<std::string, int>>;

struct ConflictoLL1 {
    std::string noTerminal;
    std::string terminal;
    int produccionExistente;
    int produccionNueva;
};

struct ResultadoTablaLL1 {
    TablaLL1 tabla;
    std::vector<ConflictoLL1> conflictos;
    bool esLL1() const { return conflictos.empty(); }
};

ResultadoTablaLL1 construirTablaLL1(
    const Gramatica& gramatica,
    const MapaFirst& first,
    const MapaFollow& follow
);

void imprimirTablaLL1(
    const ResultadoTablaLL1& resultado,
    const Gramatica& gramatica
);

void imprimirConflictos(
    const std::vector<ConflictoLL1>& conflictos,
    const Gramatica& gramatica
);

// ─── Evaluador con recuperación de errores ────────────────────────────────────
// erroresEncontrados: se llena con el número de errores recuperados.
// Retorna true solo si NO hubo errores (aceptación limpia).

bool evaluarLL1ConRecuperacion(
    const ResultadoTablaLL1& tabla,
    const Gramatica& gramatica,
    const std::vector<Token>& tokens,
    const std::vector<TokenPosicion>& posiciones,
    int& pasos,
    bool verbose,
    int& erroresEncontrados
);

#endif
