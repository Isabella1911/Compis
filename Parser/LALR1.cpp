#include "LALR1.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>

static const std::string DUMMY = "#";

// ─── Clausura LR(1) ───────────────────────────────────────────────────────────

static ConjuntoItemsLR1 clausuraLR1(const ConjuntoItemsLR1& items,
                                     const Gramatica& g,
                                     const MapaFirst& first) {
    ConjuntoItemsLR1 resultado = items;
    bool cambio = true;

    while (cambio) {
        cambio = false;
        ConjuntoItemsLR1 nuevos;

        for (const ItemLR1& item : resultado) {
            const Produccion& prod = g.producciones[static_cast<size_t>(item.produccion)];
            bool esEps = (prod.derecha.size() == 1 && prod.derecha[0] == "epsilon");
            if (esEps || item.punto >= static_cast<int>(prod.derecha.size())) continue;

            std::string B = prod.derecha[static_cast<size_t>(item.punto)];
            if (!g.noTerminales.count(B)) continue;

            // Construir secuencia β + lookahead
            std::vector<std::string> beta;
            for (int k = item.punto + 1; k < static_cast<int>(prod.derecha.size()); k++)
                beta.push_back(prod.derecha[static_cast<size_t>(k)]);

            ConjuntoSimbolos firstBeta;
            if (item.lookahead == DUMMY) {
                // Con lookahead dummy: calcular FIRST(β), si tiene epsilon → propagar DUMMY
                if (beta.empty()) {
                    firstBeta.insert(DUMMY);
                } else {
                    firstBeta = firstDeSecuencia(beta, first);
                    if (firstBeta.count("epsilon")) {
                        firstBeta.erase("epsilon");
                        firstBeta.insert(DUMMY);
                    }
                }
            } else {
                // Lookahead real: calcular FIRST(β a)
                std::vector<std::string> betaA = beta;
                betaA.push_back(item.lookahead);
                firstBeta = firstDeSecuencia(betaA, first);
                firstBeta.erase("epsilon");
            }

            for (int i = 0; i < static_cast<int>(g.producciones.size()); i++) {
                if (g.producciones[static_cast<size_t>(i)].izquierda != B) continue;
                for (const std::string& b : firstBeta) {
                    ItemLR1 nuevo{i, 0, b};
                    if (!resultado.count(nuevo) && !nuevos.count(nuevo))
                        nuevos.insert(nuevo);
                }
            }
        }

        if (!nuevos.empty()) {
            for (const auto& n : nuevos) resultado.insert(n);
            cambio = true;
        }
    }

    return resultado;
}

// ─── Cálculo de lookaheads LALR(1) ───────────────────────────────────────────

std::map<int, std::map<ItemLR0, std::set<std::string>>>
calcularLookaheadsLALR(const AutomataLR0& automata) {
    const Gramatica& g = automata.gramaticaAumentada;
    MapaFirst first = calcularFirst(g);

    std::map<int, std::map<ItemLR0, std::set<std::string>>> lookaheads;

    // Item inicial S' -> • S tiene lookahead "$"
    ItemLR0 itemInicial{0, 0};
    lookaheads[0][itemInicial].insert("$");

    using Prop = std::pair<std::pair<int,ItemLR0>, std::pair<int,ItemLR0>>;
    std::vector<Prop> propagaciones;

    int n = static_cast<int>(automata.estados.size());

    for (int i = 0; i < n; i++) {
        const ConjuntoItems& estadoI = automata.estados[static_cast<size_t>(i)];

        for (const ItemLR0& kernelItem : estadoI) {
            // Clausura LR(1) con dummy para detectar generación espontánea y propagación
            ConjuntoItemsLR1 J;
            J.insert(ItemLR1{kernelItem.produccion, kernelItem.punto, DUMMY});
            ConjuntoItemsLR1 clausJ = clausuraLR1(J, g, first);

            for (const ItemLR1& lr1item : clausJ) {
                const Produccion& prod = g.producciones[static_cast<size_t>(lr1item.produccion)];
                bool esEps = (prod.derecha.size() == 1 && prod.derecha[0] == "epsilon");
                if (esEps || lr1item.punto >= static_cast<int>(prod.derecha.size())) continue;

                std::string X = prod.derecha[static_cast<size_t>(lr1item.punto)];

                auto it = automata.goto_[static_cast<size_t>(i)].find(X);
                if (it == automata.goto_[static_cast<size_t>(i)].end()) continue;
                int j = it->second;

                ItemLR0 destItem{lr1item.produccion, lr1item.punto + 1};

                if (lr1item.lookahead == DUMMY) {
                    propagaciones.push_back({{i, kernelItem}, {j, destItem}});
                } else {
                    lookaheads[j][destItem].insert(lr1item.lookahead);
                }
            }
        }
    }

    // Propagar hasta punto fijo
    bool cambio = true;
    while (cambio) {
        cambio = false;
        for (const Prop& p : propagaciones) {
            const auto& [origen, destino] = p;
            const auto& [io, ko] = origen;
            const auto& [id, kd] = destino;

            auto& conjOrigen  = lookaheads[io][ko];
            auto& conjDestino = lookaheads[id][kd];

            for (const std::string& la : conjOrigen) {
                if (la == DUMMY) continue;
                if (!conjDestino.count(la)) {
                    conjDestino.insert(la);
                    cambio = true;
                }
            }
        }
    }

    return lookaheads;
}

// ─── Construcción de la tabla LALR(1) ────────────────────────────────────────

TablaLALR1 construirLALR1(const AutomataLR0& automata, const MapaFirst& /*firstOrig*/) {
    TablaLALR1 tabla;
    tabla.gramatica = automata.gramaticaAumentada;
    const Gramatica& ga = automata.gramaticaAumentada;

    MapaFirst firstGA = calcularFirst(ga);
    MapaFollow followGA = calcularFollow(ga, firstGA);

    // Para FOLLOW necesitamos la gramática original (sin S')
    // Construimos FOLLOW sobre ga pero usaremos solo no-terminales originales
    auto lookaheads = calcularLookaheadsLALR(automata);

    size_t n = automata.estados.size();
    tabla.action.resize(n);
    tabla.goto_.resize(n);

    // GOTO sobre no-terminales
    for (size_t i = 0; i < n; i++)
        for (const auto& [sym, dest] : automata.goto_[i])
            if (ga.noTerminales.count(sym))
                tabla.goto_[i][sym] = dest;

    // ACTION
    for (size_t i = 0; i < n; i++) {
        const ConjuntoItems& estado = automata.estados[i];

        for (const ItemLR0& item : estado) {
            const Produccion& prod = ga.producciones[static_cast<size_t>(item.produccion)];
            bool esEpsilon = (prod.derecha.size() == 1 && prod.derecha[0] == "epsilon");
            bool completo  = esItemCompleto(item, ga);

            if (!completo && !esEpsilon) {
                // SHIFT
                std::string s = simboloTrasElPunto(item, ga);
                if (!s.empty() && ga.terminales.count(s)) {
                    auto it = automata.goto_[i].find(s);
                    if (it != automata.goto_[i].end()) {
                        AccionSLR nueva{TipoAccion::SHIFT, it->second};
                        AccionSLR& celda = tabla.action[i][s];
                        if (!celda.esError() && (celda.tipo != nueva.tipo || celda.valor != nueva.valor)) {
                            std::string desc = (celda.tipo == TipoAccion::REDUCE)
                                               ? "shift/reduce conflict" : "shift/shift conflict";
                            tabla.conflictos.push_back({static_cast<int>(i), s, celda, nueva, desc});
                        } else {
                            celda = nueva;
                        }
                    }
                }
            } else {
                // REDUCE o ACCEPT
                if (item.produccion == automata.produccionAceptacion && item.punto == 1) {
                    tabla.action[i]["$"] = AccionSLR{TipoAccion::ACCEPT, -1};
                } else {
                    // Determinar el conjunto de lookaheads para este reduce.
                    // Primero intentar los lookaheads LALR calculados.
                    // Para items epsilon (punto=0), usar FOLLOW como respaldo
                    // porque la propagación no alcanza items de clausura.
                    std::set<std::string> las;

                    auto itLA = lookaheads.find(static_cast<int>(i));
                    if (itLA != lookaheads.end()) {
                        auto itItem = itLA->second.find(item);
                        if (itItem != itLA->second.end()) {
                            for (const std::string& la : itItem->second)
                                if (la != DUMMY) las.insert(la);
                        }
                    }

                    // Si no obtuvimos lookaheads (item generado por clausura, no kernel),
                    // usar FOLLOW del no-terminal izquierdo como respaldo
                    if (las.empty()) {
                        const std::string& A = prod.izquierda;
                        auto itF = followGA.find(A);
                        if (itF != followGA.end())
                            las = itF->second;
                    }

                    for (const std::string& la : las) {
                        if (la == DUMMY) continue;
                        AccionSLR nueva{TipoAccion::REDUCE, item.produccion};
                        AccionSLR& celda = tabla.action[i][la];
                        if (!celda.esError() && (celda.tipo != nueva.tipo || celda.valor != nueva.valor)) {
                            std::string desc = (celda.tipo == TipoAccion::SHIFT)
                                               ? "shift/reduce conflict" : "reduce/reduce conflict";
                            tabla.conflictos.push_back({static_cast<int>(i), la, celda, nueva, desc});
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

// ─── Evaluador y impresión ───────────────────────────────────────────────────

bool evaluarLALR1(const TablaLALR1& tabla,
                  const std::vector<Token>& tokens,
                  const std::vector<TokenPosicion>& posiciones,
                  bool verbose,
                  int& erroresEncontrados) {
    return evaluarSLR1(tabla, tokens, posiciones, verbose, erroresEncontrados);
}

void imprimirTablaLALR1(const TablaLALR1& tabla) {
    std::cout << "Tabla LALR(1):\n";
    imprimirCuerpoTablaSLR1(tabla);
    if (tabla.conflictos.empty()) {
        std::cout << "\n  La gramatica es LALR(1).\n";
    } else {
        std::cout << "\n  *** La gramatica NO es LALR(1). Conflictos: "
                  << tabla.conflictos.size() << " ***\n";
        imprimirConflictosSLR(tabla.conflictos);
    }
}