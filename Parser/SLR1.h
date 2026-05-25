#ifndef SLR1_H
#define SLR1_H

#include "LR0.h"
#include "FirstFollow.h"
#include "../lexer/Token.h"
#include <map>
#include <string>
#include <vector>

enum class TipoAccion { SHIFT, REDUCE, ACCEPT, ERROR };

struct AccionSLR {
    TipoAccion tipo  = TipoAccion::ERROR;
    int        valor = -1;

    bool esError()  const { return tipo == TipoAccion::ERROR;  }
    bool esAccept() const { return tipo == TipoAccion::ACCEPT; }
};

struct ConflictoSLR {
    int         estado;
    std::string terminal;
    AccionSLR   accionExistente;
    AccionSLR   accionNueva;
    std::string descripcion;
};

struct TablaSLR1 {
    std::vector<std::map<std::string, AccionSLR>> action;
    std::vector<std::map<std::string, int>>       goto_;
    std::vector<ConflictoSLR>                     conflictos;
    Gramatica                                     gramatica;

    bool esSLR1() const { return conflictos.empty(); }
};

TablaSLR1 construirSLR1(const AutomataLR0& automata, const MapaFollow& follow);

bool evaluarSLR1(const TablaSLR1& tabla,
                 const std::vector<Token>& tokens,
                 const std::vector<TokenPosicion>& posiciones,
                 bool verbose,
                 int& erroresEncontrados);

void imprimirTablaSLR1(const TablaSLR1& tabla);
void imprimirCuerpoTablaSLR1(const TablaSLR1& tabla);
void imprimirConflictosSLR(const std::vector<ConflictoSLR>& conflictos);

#endif // SLR1_H