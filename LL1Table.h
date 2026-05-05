#ifndef LL1TABLE_H
#define LL1TABLE_H

#include "Grammar.h"
#include "FirstFollow.h"
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

// Tabla LL(1): M[noTerminal][terminal] = índice de producción en Gramatica::producciones.
// -1 significa celda vacía (error sintáctico).
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

// Construye la tabla LL(1) y registra todos los conflictos encontrados.
// No lanza excepción en conflictos: los acumula en ResultadoTablaLL1.
ResultadoTablaLL1 construirTablaLL1(
    const Gramatica& gramatica,
    const MapaFirst& first,
    const MapaFollow& follow
);

// Imprime la tabla LL(1) en formato legible.
void imprimirTablaLL1(
    const ResultadoTablaLL1& resultado,
    const Gramatica& gramatica
);

// Imprime los conflictos detectados.
void imprimirConflictos(
    const std::vector<ConflictoLL1>& conflictos,
    const Gramatica& gramatica
);

#endif
