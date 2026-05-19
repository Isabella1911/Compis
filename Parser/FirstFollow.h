#ifndef FIRSTFOLLOW_H
#define FIRSTFOLLOW_H

#include "Grammar.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>

using ConjuntoSimbolos = std::set<std::string>;
using MapaFirst        = std::map<std::string, ConjuntoSimbolos>;
using MapaFollow       = std::map<std::string, ConjuntoSimbolos>;

// Agrega todos los elementos de 'origen' a 'destino', omitiendo epsilon.
// Retorna true si 'destino' cambió.
bool agregarSinEpsilon(ConjuntoSimbolos& destino, const ConjuntoSimbolos& origen);

// FIRST de una secuencia de símbolos (necesario para construir la tabla LL(1)).
ConjuntoSimbolos firstDeSecuencia(
    const std::vector<std::string>& secuencia,
    const MapaFirst& first
);

// Calcula FIRST para todos los terminales y no terminales de la gramática.
MapaFirst calcularFirst(const Gramatica& g);

// Calcula FOLLOW para todos los no terminales de la gramática.
// Requiere FIRST ya calculado.
MapaFollow calcularFollow(const Gramatica& g, const MapaFirst& first);

// Imprime FIRST y FOLLOW de forma legible.
void imprimirFirst(const MapaFirst& first, const Gramatica& g);
void imprimirFollow(const MapaFollow& follow);

#endif
