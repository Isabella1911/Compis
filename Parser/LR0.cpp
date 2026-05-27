#include "LR0.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

// ─── Utilidades ───────────────────────────────────────────────────────────────

std::string simboloTrasElPunto(const ItemLR0& item, const Gramatica& g) {
    const Produccion& prod = g.producciones[static_cast<size_t>(item.produccion)];
    if (prod.derecha.size() == 1 && prod.derecha[0] == "epsilon")
        return "";
    if (item.punto < static_cast<int>(prod.derecha.size()))
        return prod.derecha[static_cast<size_t>(item.punto)];
    return "";
}

bool esItemCompleto(const ItemLR0& item, const Gramatica& g) {
    return simboloTrasElPunto(item, g).empty();
}

// ─── Clausura ─────────────────────────────────────────────────────────────────

ConjuntoItems clausura(const ConjuntoItems& items, const Gramatica& g) {
    ConjuntoItems resultado = items;
    bool cambio = true;

    while (cambio) {
        cambio = false;
        ConjuntoItems nuevos;

        for (const ItemLR0& item : resultado) {
            std::string B = simboloTrasElPunto(item, g);
            if (B.empty()) continue;

            // Si B es no terminal, agregar todos sus items iniciales
            if (g.noTerminales.count(B)) {
                for (int i = 0; i < static_cast<int>(g.producciones.size()); i++) {
                    if (g.producciones[static_cast<size_t>(i)].izquierda == B) {
                        ItemLR0 nuevo{i, 0};
                        if (!resultado.count(nuevo) && !nuevos.count(nuevo)) {
                            nuevos.insert(nuevo);
                        }
                    }
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

// ─── GOTO ─────────────────────────────────────────────────────────────────────

ConjuntoItems gotoConjunto(const ConjuntoItems& items,
                            const std::string& simbolo,
                            const Gramatica& g) {
    ConjuntoItems kernel;
    for (const ItemLR0& item : items) {
        std::string s = simboloTrasElPunto(item, g);
        if (s == simbolo)
            kernel.insert(ItemLR0{item.produccion, item.punto + 1});
    }
    if (kernel.empty()) return {};
    return clausura(kernel, g);
}

// ─── Gramática aumentada ──────────────────────────────────────────────────────

static Gramatica aumentarGramatica(const Gramatica& g) {
    Gramatica aumentada = g;
    std::string nuevaRaiz = g.simboloInicial + "'";
    Produccion produccionInicial(nuevaRaiz, {g.simboloInicial});
    aumentada.producciones.insert(aumentada.producciones.begin(), produccionInicial);
    aumentada.noTerminales.insert(nuevaRaiz);
    aumentada.simboloInicial = nuevaRaiz;
    return aumentada;
}

// ─── Construcción del autómata LR(0) ─────────────────────────────────────────

AutomataLR0 construirLR0(const Gramatica& g) {
    AutomataLR0 automata;
    automata.gramaticaAumentada = aumentarGramatica(g);
    const Gramatica& ga = automata.gramaticaAumentada;

    // Estado inicial: clausura de [S' -> • S]
    ConjuntoItems inicial;
    inicial.insert(ItemLR0{0, 0});
    ConjuntoItems estadoInicial = clausura(inicial, ga);

    automata.estados.push_back(estadoInicial);
    automata.goto_.push_back({});

    int procesado = 0;
    while (procesado < static_cast<int>(automata.estados.size())) {
        const ConjuntoItems estado = automata.estados[static_cast<size_t>(procesado)];

        // Recopilar TODOS los símbolos posibles tras el punto
        std::set<std::string> simbolos;
        for (const ItemLR0& item : estado) {
            std::string s = simboloTrasElPunto(item, ga);
            if (!s.empty()) simbolos.insert(s);
        }

        for (const std::string& sym : simbolos) {
            ConjuntoItems destino = gotoConjunto(estado, sym, ga);
            if (destino.empty()) continue;

            // Buscar si ya existe un estado igual
            int idxDestino = -1;
            for (int i = 0; i < static_cast<int>(automata.estados.size()); i++) {
                if (automata.estados[static_cast<size_t>(i)] == destino) {
                    idxDestino = i;
                    break;
                }
            }

            if (idxDestino == -1) {
                idxDestino = static_cast<int>(automata.estados.size());
                automata.estados.push_back(destino);
                automata.goto_.push_back({});
            }

            automata.goto_[static_cast<size_t>(procesado)][sym] = idxDestino;
        }

        procesado++;
    }

    automata.produccionAceptacion = 0;
    return automata;
}

// ─── Impresión ────────────────────────────────────────────────────────────────

static std::string itemAString(const ItemLR0& item, const Gramatica& g) {
    const Produccion& prod = g.producciones[static_cast<size_t>(item.produccion)];
    std::string s = prod.izquierda + " ->";

    bool esEpsilon = (prod.derecha.size() == 1 && prod.derecha[0] == "epsilon");

    if (esEpsilon) {
        s += (item.punto == 0) ? " •" : " ε •";
    } else {
        for (int i = 0; i <= static_cast<int>(prod.derecha.size()); i++) {
            if (i == item.punto) s += " •";
            if (i < static_cast<int>(prod.derecha.size()))
                s += " " + prod.derecha[static_cast<size_t>(i)];
        }
    }
    return s;
}

void imprimirConjunto(const ConjuntoItems& items,
                      const Gramatica& g,
                      int numeroEstado) {
    if (numeroEstado >= 0)
        std::cout << "Estado " << numeroEstado << ":\n";
    for (const ItemLR0& item : items)
        std::cout << "  [ " << itemAString(item, g) << " ]\n";
}

void imprimirLR0(const AutomataLR0& automata) {
    const Gramatica& g = automata.gramaticaAumentada;

    std::cout << "Gramática aumentada:\n";
    for (size_t i = 0; i < g.producciones.size(); i++) {
        const Produccion& p = g.producciones[i];
        std::cout << "  " << i << ": " << p.izquierda << " ->";
        for (const auto& s : p.derecha) std::cout << " " << s;
        std::cout << "\n";
    }
    std::cout << "\n";

    std::cout << "Conjuntos canónicos (" << automata.estados.size() << " estados):\n\n";

    for (size_t i = 0; i < automata.estados.size(); i++) {
        imprimirConjunto(automata.estados[i], g, static_cast<int>(i));
        const auto& trans = automata.goto_[i];
        for (const auto& [sym, dest] : trans)
            std::cout << "  GOTO(" << i << ", " << sym << ") = " << dest << "\n";
        std::cout << "\n";
    }
}

// ─── Exportación a Graphviz (.dot) ───────────────────────────────────────────

static std::string escaparDot(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '<':  r += "\\<";  break;
            case '>':  r += "\\>";  break;
            case '|':  r += "\\|";  break;
            case '{':  r += "\\{";  break;
            case '}':  r += "\\}";  break;
            case '\n': r += "\\l";  break;
            default:   r += c;       break;
        }
    }
    return r;
}

bool exportarLR0Dot(const AutomataLR0& automata, const std::string& ruta) {
    std::ofstream out(ruta);
    if (!out.is_open()) return false;

    const Gramatica& g = automata.gramaticaAumentada;

    out << "digraph LR0 {\n";
    out << "  rankdir=LR;\n";
    out << "  node [shape=record, fontname=\"Consolas\", fontsize=10];\n";
    out << "  edge [fontname=\"Consolas\", fontsize=10];\n\n";

    for (size_t i = 0; i < automata.estados.size(); i++) {
        std::ostringstream label;
        label << "I" << i << "\\l";
        for (const ItemLR0& item : automata.estados[i]) {
            label << itemAString(item, g) << "\\l";
        }
        out << "  s" << i << " [label=\"{" << escaparDot(label.str()) << "}\"];\n";
    }

    out << "\n";

    for (size_t i = 0; i < automata.estados.size(); i++) {
        for (const auto& [sym, dest] : automata.goto_[i]) {
            out << "  s" << i << " -> s" << dest
                << " [label=\"" << escaparDot(sym) << "\"];\n";
        }
    }

    out << "}\n";
    return true;
}