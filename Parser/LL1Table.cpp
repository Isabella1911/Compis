#include "LL1Table.h"
#include <iostream>
#include <iomanip>

ResultadoTablaLL1 construirTablaLL1(
    const Gramatica& gramatica,
    const MapaFirst& first,
    const MapaFollow& follow)
{
    ResultadoTablaLL1 resultado;

    // Inicializar todas las celdas en -1 (vacío)
    for (const std::string& nt : gramatica.noTerminales) {
        for (const std::string& t : gramatica.terminales) {
            resultado.tabla[nt][t] = -1;
        }
    }

    for (size_t i = 0; i < gramatica.producciones.size(); i++) {
        const Produccion& p = gramatica.producciones[i];
        const std::string& A = p.izquierda;

        ConjuntoSimbolos firstAlpha = firstDeSecuencia(p.derecha, first);

        // Para cada terminal en FIRST(alpha) \ {epsilon}, agregar M[A, t] = i
        for (const std::string& t : firstAlpha) {
            if (t == "epsilon") continue;

            int& celda = resultado.tabla[A][t];
            if (celda != -1 && celda != static_cast<int>(i)) {
                resultado.conflictos.push_back({A, t, celda, static_cast<int>(i)});
                // Conservar la primera producción (no sobrescribir silenciosamente)
            } else {
                celda = static_cast<int>(i);
            }
        }

        // Si epsilon está en FIRST(alpha), usar FOLLOW(A)
        if (firstAlpha.count("epsilon") > 0) {
            auto itFollow = follow.find(A);
            if (itFollow != follow.end()) {
                for (const std::string& t : itFollow->second) {
                    int& celda = resultado.tabla[A][t];
                    if (celda != -1 && celda != static_cast<int>(i)) {
                        resultado.conflictos.push_back({A, t, celda, static_cast<int>(i)});
                    } else {
                        celda = static_cast<int>(i);
                    }
                }
            }
        }
    }

    return resultado;
}

static std::string produccionAString(const Produccion& p) {
    std::string s = p.izquierda + " ->";
    for (const std::string& sym : p.derecha) {
        s += " " + sym;
    }
    return s;
}

void imprimirTablaLL1(
    const ResultadoTablaLL1& resultado,
    const Gramatica& gramatica)
{
    std::cout << "Tabla LL(1):\n";

    for (const std::string& nt : gramatica.noTerminales) {
        auto itNT = resultado.tabla.find(nt);
        if (itNT == resultado.tabla.end()) continue;

        for (const std::string& t : gramatica.terminales) {
            auto itT = itNT->second.find(t);
            if (itT == itNT->second.end() || itT->second == -1) continue;

            const Produccion& p = gramatica.producciones[static_cast<size_t>(itT->second)];
            std::cout << "  M[" << nt << ", " << t << "] = "
                      << produccionAString(p) << "\n";
        }
    }

    if (!resultado.conflictos.empty()) {
        std::cout << "\n  *** La gramática NO es LL(1). Conflictos detectados: "
                  << resultado.conflictos.size() << " ***\n";
    } else {
        std::cout << "\n  La gramática es LL(1).\n";
    }
}

void imprimirConflictos(
    const std::vector<ConflictoLL1>& conflictos,
    const Gramatica& gramatica)
{
    if (conflictos.empty()) return;

    std::cout << "Conflictos LL(1):\n";
    for (const ConflictoLL1& c : conflictos) {
        std::cout << "  Conflicto en M[" << c.noTerminal << ", " << c.terminal << "]:\n";
        std::cout << "    ya existe: "
                  << produccionAString(gramatica.producciones[static_cast<size_t>(c.produccionExistente)])
                  << "\n";
        std::cout << "    se intentó agregar: "
                  << produccionAString(gramatica.producciones[static_cast<size_t>(c.produccionNueva)])
                  << "\n";
    }
}
