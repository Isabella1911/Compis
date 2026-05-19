#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <string>
#include <vector>
#include <set>
#include <stdexcept>

struct Produccion {
    std::string izquierda;
    std::vector<std::string> derecha;

    Produccion(const std::string& izq, const std::vector<std::string>& der)
        : izquierda(izq), derecha(der) {}
};

struct Gramatica {
    std::set<std::string> terminales;
    std::set<std::string> noTerminales;
    std::vector<Produccion> producciones;
    std::string simboloInicial;
};

bool esToken(const std::string& simbolo);
bool esNoTerminal(const std::string& simbolo);
void validarGramatica(const Gramatica& g);

#endif
