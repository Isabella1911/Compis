#ifndef LR0_H
#define LR0_H

#include "Grammar.h"
#include <vector>
#include <set>
#include <map>
#include <string>

struct ItemLR0 {
    int produccion;
    int punto;

    bool operator<(const ItemLR0& o) const {
        if (produccion != o.produccion) return produccion < o.produccion;
        return punto < o.punto;
    }
    bool operator==(const ItemLR0& o) const {
        return produccion == o.produccion && punto == o.punto;
    }
};

using ConjuntoItems = std::set<ItemLR0>;

struct AutomataLR0 {
    Gramatica gramaticaAumentada;
    std::vector<ConjuntoItems> estados;
    std::vector<std::map<std::string, int>> goto_;
    int produccionAceptacion = 0;
};

AutomataLR0    construirLR0(const Gramatica& g);
ConjuntoItems  clausura(const ConjuntoItems& items, const Gramatica& g);
ConjuntoItems  gotoConjunto(const ConjuntoItems& items, const std::string& simbolo, const Gramatica& g);
std::string    simboloTrasElPunto(const ItemLR0& item, const Gramatica& g);
bool           esItemCompleto(const ItemLR0& item, const Gramatica& g);
void           imprimirLR0(const AutomataLR0& automata);
void           imprimirConjunto(const ConjuntoItems& items, const Gramatica& g, int numeroEstado = -1);

// Exporta el autómata LR(0) a formato DOT (Graphviz).
// Cada nodo es un estado con sus items y cada arista es una transición GOTO.
// Retorna true si pudo escribir el archivo.
bool           exportarLR0Dot(const AutomataLR0& automata, const std::string& ruta);

#endif // LR0_H