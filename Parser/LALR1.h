#ifndef LALR1_H
#define LALR1_H

#include "LR0.h"
#include "FirstFollow.h"
#include "SLR1.h"
#include "../lexer/Token.h"
#include <map>
#include <set>
#include <string>
#include <vector>

struct ItemLR1 {
    int         produccion;
    int         punto;
    std::string lookahead;

    bool operator<(const ItemLR1& o) const {
        if (produccion != o.produccion) return produccion < o.produccion;
        if (punto      != o.punto)      return punto      < o.punto;
        return lookahead < o.lookahead;
    }
};

using ConjuntoItemsLR1 = std::set<ItemLR1>;
using TablaLALR1       = TablaSLR1;

std::map<int, std::map<ItemLR0, std::set<std::string>>>
calcularLookaheadsLALR(const AutomataLR0& automata);

TablaLALR1 construirLALR1(const AutomataLR0& automata, const MapaFirst& first);

bool evaluarLALR1(const TablaLALR1& tabla,
                  const std::vector<Token>& tokens,
                  const std::vector<TokenPosicion>& posiciones,
                  bool verbose,
                  int& erroresEncontrados);

void imprimirTablaLALR1(const TablaLALR1& tabla);

// Veredicto LALR(1): mismo criterio de ausencia de conflictos que SLR(1),
// pero la tabla LALR(1) usa lookaheads refinados. Exponerlo aquí permite
// que el resto del pipeline use el nombre semánticamente correcto.
inline bool esLALR1(const TablaLALR1& tabla) {
    return tabla.conflictos.empty();
}

#endif // LALR1_H