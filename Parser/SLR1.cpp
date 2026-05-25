#include "SLR1.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// ─── Utilidad: convertir acción a string ─────────────────────────────────────

static std::string accionAString(const AccionSLR& a) {
    switch (a.tipo) {
        case TipoAccion::SHIFT:  return "s" + std::to_string(a.valor);
        case TipoAccion::REDUCE: return "r" + std::to_string(a.valor);
        case TipoAccion::ACCEPT: return "acc";
        case TipoAccion::ERROR:  return "";
    }
    return "";
}

// ─── Construcción de la tabla SLR(1) ─────────────────────────────────────────

TablaSLR1 construirSLR1(const AutomataLR0& automata, const MapaFollow& follow) {
    TablaSLR1 tabla;
    tabla.gramatica = automata.gramaticaAumentada;
    const Gramatica& ga = automata.gramaticaAumentada;

    size_t n = automata.estados.size();
    tabla.action.resize(n);
    tabla.goto_.resize(n);

    // ── Llenar GOTO con transiciones sobre no-terminales ─────────────────────
    for (size_t i = 0; i < n; i++) {
        for (const auto& [sym, dest] : automata.goto_[i]) {
            if (ga.noTerminales.count(sym)) {
                tabla.goto_[i][sym] = dest;
            }
        }
    }

    // ── Llenar ACTION ─────────────────────────────────────────────────────────
    for (size_t i = 0; i < n; i++) {
        const ConjuntoItems& estado = automata.estados[i];

        for (const ItemLR0& item : estado) {
            const Produccion& prod = ga.producciones[static_cast<size_t>(item.produccion)];

            bool esEpsilon = (prod.derecha.size() == 1 && prod.derecha[0] == "epsilon");
            bool completo  = esItemCompleto(item, ga);

            if (!completo && !esEpsilon) {
                // ── SHIFT ────────────────────────────────────────────────────
                std::string s = simboloTrasElPunto(item, ga);
                if (!s.empty() && ga.terminales.count(s)) {
                    // Hay transición GOTO sobre terminal → shift
                    auto it = automata.goto_[i].find(s);
                    if (it != automata.goto_[i].end()) {
                        AccionSLR nueva{TipoAccion::SHIFT, it->second};
                        AccionSLR& celda = tabla.action[i][s];

                        if (!celda.esError() && celda.tipo != nueva.tipo) {
                            tabla.conflictos.push_back({
                                static_cast<int>(i), s, celda, nueva,
                                "shift/reduce conflict"
                            });
                        } else {
                            celda = nueva;
                        }
                    }
                }
            } else {
                // ── REDUCE o ACCEPT ───────────────────────────────────────────
                if (item.produccion == automata.produccionAceptacion) {
                    // Item de aceptación: S' -> S •
                    // Solo si el punto está al final (posición 1 para S' -> S)
                    if (item.punto == 1) {
                        AccionSLR acc{TipoAccion::ACCEPT, -1};
                        tabla.action[i]["$"] = acc;
                    }
                } else {
                    // Reduce: buscar el no-terminal del lado izquierdo en la gramática ORIGINAL
                    // La producción 0 de la aumentada es S' -> S (índice 0)
                    // Las demás producciones originales están desde el índice 1
                    const std::string& A = prod.izquierda;

                    // FOLLOW se calculó sobre la gramática original.
                    // Necesitamos mapear el nombre del no-terminal.
                    // Como la gramática aumentada solo agregó S', usamos FOLLOW directamente
                    // para todos los no-terminales originales.
                    // Para S' no necesitamos reduce (ya se maneja con accept).

                    auto itFollow = follow.find(A);
                    if (itFollow == follow.end()) continue; // S' no tiene FOLLOW en original

                    for (const std::string& terminal : itFollow->second) {
                        // Índice de reduce = índice en gramática aumentada
                        AccionSLR nueva{TipoAccion::REDUCE, item.produccion};
                        AccionSLR& celda = tabla.action[i][terminal];

                        if (!celda.esError()) {
                            if (celda.tipo != nueva.tipo || celda.valor != nueva.valor) {
                                std::string desc = (celda.tipo == TipoAccion::SHIFT)
                                                   ? "shift/reduce conflict"
                                                   : "reduce/reduce conflict";
                                tabla.conflictos.push_back({
                                    static_cast<int>(i), terminal, celda, nueva, desc
                                });
                            }
                        } else {
                            celda = nueva;
                        }
                    }
                }
            }
        }
    }

    return tabla;
}

// ─── Evaluador SLR(1) ─────────────────────────────────────────────────────────

bool evaluarSLR1(const TablaSLR1& tabla,
                 const std::vector<Token>& tokens,
                 const std::vector<TokenPosicion>& posiciones,
                 bool verbose) {
    const Gramatica& g = tabla.gramatica;

    std::vector<int>         pilaEstados;
    std::vector<std::string> pilaSimbolos;
    pilaEstados.push_back(0);

    size_t idx = 0; // índice en tokens

    auto tokenActual = [&]() -> std::string {
        if (idx < tokens.size()) return tokens[idx].id;
        return "$";
    };

    auto posActual = [&]() -> TokenPosicion {
        if (idx < posiciones.size()) return posiciones[idx];
        return {-1, -1};
    };

    if (verbose) {
        std::cout << std::left
                  << std::setw(35) << "Pila (estados)"
                  << std::setw(25) << "Token actual"
                  << "Acción\n";
        std::cout << std::string(75, '-') << "\n";
    }

    const int MAX_PASOS = 10000;
    int pasos = 0;

    while (pasos++ < MAX_PASOS) {
        int estadoActual = pilaEstados.back();
        std::string tok  = tokenActual();

        // Representar la pila para impresión
        std::string pilaStr;
        for (int s : pilaEstados) pilaStr += std::to_string(s) + " ";

        // Buscar acción
        auto itEstado = tabla.action[static_cast<size_t>(estadoActual)].find(tok);
        AccionSLR accion;
        if (itEstado != tabla.action[static_cast<size_t>(estadoActual)].end()) {
            accion = itEstado->second;
        }

        if (verbose) {
            std::cout << std::left
                      << std::setw(35) << pilaStr
                      << std::setw(25) << tok
                      << accionAString(accion) << "\n";
        }

        if (accion.esAccept()) {
            if (verbose) std::cout << "\nCadena ACEPTADA.\n";
            return true;
        }

        if (accion.esError()) {
            TokenPosicion pos = posActual();
            std::cerr << "ERROR SINTÁCTICO";
            if (pos.linea > 0)
                std::cerr << " en linea " << pos.linea << ", col " << pos.columna;
            std::cerr << ": token inesperado '" << tok << "' en estado " << estadoActual << "\n";

            // Mostrar qué se esperaba
            const auto& fila = tabla.action[static_cast<size_t>(estadoActual)];
            if (!fila.empty()) {
                std::cerr << "  Se esperaba uno de: ";
                for (const auto& [t, _] : fila) std::cerr << t << " ";
                std::cerr << "\n";
            }
            return false;
        }

        if (accion.tipo == TipoAccion::SHIFT) {
            pilaSimbolos.push_back(tok);
            pilaEstados.push_back(accion.valor);
            idx++;
        } else if (accion.tipo == TipoAccion::REDUCE) {
            // Reducir con producción accion.valor
            const Produccion& prod = g.producciones[static_cast<size_t>(accion.valor)];

            // Cuántos símbolos sacar de la pila
            int longitud = static_cast<int>(prod.derecha.size());
            if (longitud == 1 && prod.derecha[0] == "epsilon") longitud = 0;

            if (static_cast<int>(pilaEstados.size()) - 1 < longitud) {
                std::cerr << "ERROR INTERNO: pila muy pequeña para reducir\n";
                return false;
            }

            for (int k = 0; k < longitud; k++) {
                pilaEstados.pop_back();
                if (!pilaSimbolos.empty()) pilaSimbolos.pop_back();
            }

            // GOTO[estadoActual después de pop][no-terminal]
            int estadoCima = pilaEstados.back();
            const std::string& A = prod.izquierda;

            auto itGoto = tabla.goto_[static_cast<size_t>(estadoCima)].find(A);
            if (itGoto == tabla.goto_[static_cast<size_t>(estadoCima)].end()) {
                std::cerr << "ERROR INTERNO: GOTO[" << estadoCima << "][" << A << "] no definido\n";
                return false;
            }

            pilaSimbolos.push_back(A);
            pilaEstados.push_back(itGoto->second);
        }
    }

    std::cerr << "ERROR: se alcanzó el límite máximo de pasos (" << MAX_PASOS << ")\n";
    return false;
}

// ─── Impresión ────────────────────────────────────────────────────────────────

void imprimirCuerpoTablaSLR1(const TablaSLR1& tabla) {
    const Gramatica& g = tabla.gramatica;

    std::vector<std::string> terms(g.terminales.begin(), g.terminales.end());
    terms.push_back("$");
    std::vector<std::string> noTerms;
    for (const auto& nt : g.noTerminales) {
        if (nt != g.simboloInicial) noTerms.push_back(nt);
    }

    int ancho = 8;

    std::cout << std::setw(6) << "Est";
    std::cout << " | ";
    std::cout << std::setw(static_cast<int>(terms.size()) * (ancho + 1)) << "ACTION";
    std::cout << " | ";
    std::cout << "GOTO\n";

    std::cout << std::setw(6) << "";
    std::cout << " | ";
    for (const auto& t : terms) std::cout << std::setw(ancho) << t << " ";
    std::cout << " | ";
    for (const auto& nt : noTerms) std::cout << std::setw(ancho) << nt << " ";
    std::cout << "\n";
    std::cout << std::string(6 + 3 + terms.size() * (ancho + 1) + 3 + noTerms.size() * (ancho + 1), '-') << "\n";

    for (size_t i = 0; i < tabla.action.size(); i++) {
        std::cout << std::setw(6) << i << " | ";

        for (const auto& t : terms) {
            auto it = tabla.action[i].find(t);
            std::string celda = (it != tabla.action[i].end()) ? accionAString(it->second) : "";
            std::cout << std::setw(ancho) << celda << " ";
        }
        std::cout << " | ";

        for (const auto& nt : noTerms) {
            auto it = tabla.goto_[i].find(nt);
            std::string celda = (it != tabla.goto_[i].end()) ? std::to_string(it->second) : "";
            std::cout << std::setw(ancho) << celda << " ";
        }
        std::cout << "\n";
    }
}

void imprimirTablaSLR1(const TablaSLR1& tabla) {
    imprimirCuerpoTablaSLR1(tabla);
    if (tabla.conflictos.empty()) {
        std::cout << "\n  La gramatica es SLR(1).\n";
    } else {
        std::cout << "\n  *** La gramatica NO es SLR(1). Conflictos: "
                  << tabla.conflictos.size() << " ***\n";
    }
}

void imprimirConflictosSLR(const std::vector<ConflictoSLR>& conflictos) {
    if (conflictos.empty()) return;

    std::cout << "Conflictos SLR(1):\n";
    for (const ConflictoSLR& c : conflictos) {
        std::cout << "  Estado " << c.estado
                  << ", terminal '" << c.terminal << "': " << c.descripcion << "\n";
        std::cout << "    acción existente : " << accionAString(c.accionExistente) << "\n";
        std::cout << "    acción nueva     : " << accionAString(c.accionNueva)     << "\n";
    }
}