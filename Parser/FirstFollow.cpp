#include "FirstFollow.h"
#include <iostream>

bool agregarSinEpsilon(ConjuntoSimbolos& destino, const ConjuntoSimbolos& origen) {
    bool cambio = false;
    for (const std::string& simbolo : origen) {
        if (simbolo == "epsilon") continue;
        if (destino.insert(simbolo).second) cambio = true;
    }
    return cambio;
}

ConjuntoSimbolos firstDeSecuencia(
    const std::vector<std::string>& secuencia,
    const MapaFirst& first)
{
    ConjuntoSimbolos resultado;

    if (secuencia.empty()) {
        resultado.insert("epsilon");
        return resultado;
    }

    bool todosConEpsilon = true;

    for (const std::string& simbolo : secuencia) {
        auto it = first.find(simbolo);
        if (it == first.end()) {
            throw std::runtime_error(
                "No existe FIRST para el símbolo: " + simbolo
            );
        }

        agregarSinEpsilon(resultado, it->second);

        if (it->second.count("epsilon") == 0) {
            todosConEpsilon = false;
            break;
        }
    }

    if (todosConEpsilon) {
        resultado.insert("epsilon");
    }

    return resultado;
}

MapaFirst calcularFirst(const Gramatica& g) {
    MapaFirst first;

    // Inicializar terminales: FIRST(a) = {a}
    for (const std::string& t : g.terminales) {
        first[t].insert(t);
    }

    // Epsilon tiene FIRST propio
    first["epsilon"].insert("epsilon");

    // Inicializar no terminales con conjunto vacío
    for (const std::string& nt : g.noTerminales) {
        first[nt] = {};
    }

    // Algoritmo iterativo hasta punto fijo
    bool cambio = true;
    while (cambio) {
        cambio = false;

        for (const Produccion& p : g.producciones) {
            ConjuntoSimbolos& firstA = first[p.izquierda];

            // Producción epsilon: A -> epsilon
            if (p.derecha.size() == 1 && p.derecha[0] == "epsilon") {
                if (firstA.insert("epsilon").second) cambio = true;
                continue;
            }

            // A -> X1 X2 ... Xn
            bool todosConEpsilon = true;

            for (const std::string& Xi : p.derecha) {
                auto it = first.find(Xi);
                if (it == first.end()) {
                    // Símbolo sin FIRST definido, no bloquear, continuar
                    todosConEpsilon = false;
                    break;
                }

                if (agregarSinEpsilon(firstA, it->second)) cambio = true;

                if (it->second.count("epsilon") == 0) {
                    todosConEpsilon = false;
                    break;
                }
            }

            if (todosConEpsilon) {
                if (firstA.insert("epsilon").second) cambio = true;
            }
        }
    }

    return first;
}

MapaFollow calcularFollow(const Gramatica& g, const MapaFirst& first) {
    MapaFollow follow;

    // Inicializar todos los no terminales con conjunto vacío
    for (const std::string& nt : g.noTerminales) {
        follow[nt] = {};
    }

    // El símbolo inicial siempre tiene $ en FOLLOW
    follow[g.simboloInicial].insert("$");

    // Algoritmo iterativo hasta punto fijo
    bool cambio = true;
    while (cambio) {
        cambio = false;

        for (const Produccion& p : g.producciones) {
            const std::string& A = p.izquierda;

            for (size_t i = 0; i < p.derecha.size(); i++) {
                const std::string& B = p.derecha[i];

                // Solo nos interesan los no terminales del lado derecho
                if (g.noTerminales.count(B) == 0) continue;

                // beta = todo lo que viene después de B en esta producción
                std::vector<std::string> beta(
                    p.derecha.begin() + static_cast<int>(i) + 1,
                    p.derecha.end()
                );

                ConjuntoSimbolos firstBeta = firstDeSecuencia(beta, first);

                // Agregar FIRST(beta) \ {epsilon} a FOLLOW(B)
                if (agregarSinEpsilon(follow[B], firstBeta)) cambio = true;

                // Si beta puede derivar epsilon, agregar FOLLOW(A) a FOLLOW(B)
                if (firstBeta.count("epsilon") > 0) {
                    auto itFollow = follow.find(A);
                    if (itFollow != follow.end()) {
                        for (const std::string& s : itFollow->second) {
                            if (follow[B].insert(s).second) cambio = true;
                        }
                    }
                }
            }
        }
    }

    return follow;
}

void imprimirFirst(const MapaFirst& first, const Gramatica& g) {
    std::cout << "FIRST:\n";
    // Solo imprimir no terminales (los terminales son triviales)
    for (const std::string& nt : g.noTerminales) {
        auto it = first.find(nt);
        std::cout << "  FIRST(" << nt << ") = { ";
        if (it != first.end()) {
            bool primero = true;
            for (const std::string& s : it->second) {
                if (!primero) std::cout << ", ";
                std::cout << s;
                primero = false;
            }
        }
        std::cout << " }\n";
    }
}

void imprimirFollow(const MapaFollow& follow) {
    std::cout << "FOLLOW:\n";
    for (const auto& par : follow) {
        std::cout << "  FOLLOW(" << par.first << ") = { ";
        bool primero = true;
        for (const std::string& s : par.second) {
            if (!primero) std::cout << ", ";
            std::cout << s;
            primero = false;
        }
        std::cout << " }\n";
    }
}
