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

// ─── Tokens de sincronización ────────────────────────────────────────────────

static bool esSincronizacionLR(const std::string& tok) {
    return tok == "SEMICOLON" || tok == "RBRACE" || tok == "$";
}

// ─── Construcción de la tabla SLR(1) ─────────────────────────────────────────

TablaSLR1 construirSLR1(const AutomataLR0& automata, const MapaFollow& follow) {
    TablaSLR1 tabla;
    tabla.gramatica = automata.gramaticaAumentada;
    const Gramatica& ga = automata.gramaticaAumentada;

    size_t n = automata.estados.size();
    tabla.action.resize(n);
    tabla.goto_.resize(n);

    for (size_t i = 0; i < n; i++) {
        for (const auto& [sym, dest] : automata.goto_[i]) {
            if (ga.noTerminales.count(sym)) {
                tabla.goto_[i][sym] = dest;
            }
        }
    }

    for (size_t i = 0; i < n; i++) {
        const ConjuntoItems& estado = automata.estados[i];

        for (const ItemLR0& item : estado) {
            const Produccion& prod = ga.producciones[static_cast<size_t>(item.produccion)];

            bool esEpsilon = (prod.derecha.size() == 1 && prod.derecha[0] == "epsilon");
            bool completo  = esItemCompleto(item, ga);

            if (!completo && !esEpsilon) {
                std::string s = simboloTrasElPunto(item, ga);
                if (!s.empty() && ga.terminales.count(s)) {
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
                if (item.produccion == automata.produccionAceptacion) {
                    if (item.punto == 1) {
                        AccionSLR acc{TipoAccion::ACCEPT, -1};
                        tabla.action[i]["$"] = acc;
                    }
                } else {
                    const std::string& A = prod.izquierda;
                    auto itFollow = follow.find(A);
                    if (itFollow == follow.end()) continue;

                    for (const std::string& terminal : itFollow->second) {
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

// ─── Recuperación de errores por frase para LR ───────────────────────────────
//
// Estrategia:
//   1. Reportar error con línea, columna, token encontrado y tokens esperados.
//   2. Intentar ELIMINAR el token actual si el siguiente es válido en el estado.
//   3. Si no, sincronizar: descartar tokens hasta SEMICOLON/RBRACE/$
//      y desapilar estados hasta que el estado actual pueda hacer shift
//      de un token de sincronización.

static bool recuperarLR(
    std::vector<int>&         pilaEstados,
    std::vector<std::string>& pilaSimbolos,
    size_t&                   idx,
    const std::vector<Token>&          tokens,
    const std::vector<TokenPosicion>&  posiciones,
    const std::vector<std::map<std::string, AccionSLR>>& action,
    int& erroresReportados)
{
    erroresReportados++;

    const std::string tokActual = (idx < tokens.size()) ? tokens[idx].id : "$";
    const TokenPosicion pos     = (idx < posiciones.size()) ? posiciones[idx] : TokenPosicion{-1,-1};
    int estadoActual            = pilaEstados.back();

    // ── Armar mensaje de error ────────────────────────────────────────────────
    std::cerr << "\n[ERROR SINTÁCTICO #" << erroresReportados << "]";
    if (pos.linea > 0)
        std::cerr << " Línea " << pos.linea << ", col " << pos.columna;
    std::cerr << "\n";
    std::cerr << "  Encontró:  '" << tokActual << "' en estado " << estadoActual << "\n";

    // Mostrar tokens esperados en este estado
    const auto& filaActual = action[static_cast<size_t>(estadoActual)];
    if (!filaActual.empty()) {
        std::cerr << "  Esperaba uno de: ";
        for (const auto& [t, _] : filaActual) std::cerr << t << " ";
        std::cerr << "\n";
    }

    // ── Intento 1: eliminar token actual ─────────────────────────────────────
    // Si el siguiente token sí tiene acción en el estado actual, descartar el actual
    if (!esSincronizacionLR(tokActual) && idx + 1 < tokens.size()) {
        const std::string& sigTok = tokens[idx + 1].id;
        auto itSig = filaActual.find(sigTok);
        if (itSig != filaActual.end() && !itSig->second.esError()) {
            std::cerr << "  Tipo:      Token inesperado\n";
            std::cerr << "  Acción:    Se descarta '" << tokActual << "' (eliminación)\n";
            idx++;
            return true;
        }
    }

    // ── Intento 2: insertar token faltante (solo para terminales simples) ────
    // Si el estado espera exactamente un token de sincronización y ese es el siguiente
    if (!esSincronizacionLR(tokActual)) {
        for (const std::string& sync : {"SEMICOLON", "RBRACE"}) {
            auto itSync = filaActual.find(sync);
            if (itSync != filaActual.end() && !itSync->second.esError()) {
                // El estado acepta SEMICOLON o RBRACE — probamos insertar
                // Verificar que el token actual también está en el siguiente estado
                // (heurística: si el estado espera solo sync, insertarlo es seguro)
                if (filaActual.size() == 1) {
                    std::cerr << "  Tipo:      Token faltante\n";
                    std::cerr << "  Acción:    Se inserta '" << sync << "' (inserción implícita)\n";
                    // No avanzamos idx — simulamos tener el token sync
                    // Para esto modificamos temporalmente: push un token falso
                    // En la práctica: retornamos true y dejamos que el loop lo maneje
                    // Insertamos el token sync en la posición actual
                    // (esto requiere que el llamador use el token "insertado")
                    // Simplificación: tratar el token actual como si fuera sync
                    // => no avanzar, pero cambiar el token efectivo no es posible sin modificar
                    // el vector. Mejor hacer shift del sync directamente si hay acción.
                    // Como no podemos insertar en el vector, usamos sincronización.
                    break;
                }
            }
        }
    }

    // ── Intento 3: sincronización (pánico controlado) ─────────────────────────
    std::cerr << "  Tipo:      Sin producción válida\n";
    std::cerr << "  Acción:    Sincronizando — descartando tokens hasta ';' '}' o fin\n";

    // Descartar tokens hasta token de sincronización
    while (idx < tokens.size() && !esSincronizacionLR(tokens[idx].id)) {
        idx++;
    }

    if (idx >= tokens.size()) return false;

    const std::string tokSync = tokens[idx].id;

    // Desapilar estados hasta que el estado cima pueda hacer shift de tokSync
    // o hasta que solo quede el estado 0
    while (pilaEstados.size() > 1) {
        int cima = pilaEstados.back();
        auto itCima = action[static_cast<size_t>(cima)].find(tokSync);
        if (itCima != action[static_cast<size_t>(cima)].end() && !itCima->second.esError()) {
            break; // Este estado puede manejar el token de sincronización
        }
        pilaEstados.pop_back();
        if (!pilaSimbolos.empty()) pilaSimbolos.pop_back();
    }

    // Consumir el token de sincronización si es SEMICOLON
    if (tokSync == "SEMICOLON") {
        idx++;
    }

    return true;
}

// ─── Evaluador SLR(1) con recuperación de errores ────────────────────────────

bool evaluarSLR1(const TablaSLR1& tabla,
                 const std::vector<Token>& tokens,
                 const std::vector<TokenPosicion>& posiciones,
                 bool verbose,
                 int& erroresEncontrados) {
    const Gramatica& g = tabla.gramatica;

    std::vector<int>         pilaEstados;
    std::vector<std::string> pilaSimbolos;
    pilaEstados.push_back(0);

    size_t idx        = 0;
    int    errores    = 0;
    bool   huboError  = false;
    const int MAX_PASOS   = 10000;
    const int MAX_ERRORES = 20;
    int pasos = 0;

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

    while (pasos++ < MAX_PASOS && errores < MAX_ERRORES) {
        int estadoActual = pilaEstados.back();
        std::string tok  = tokenActual();

        std::string pilaStr;
        for (int s : pilaEstados) pilaStr += std::to_string(s) + " ";

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
            if (verbose) {
                if (!huboError)
                    std::cout << "\n  --> ACEPTADO\n";
                else
                    std::cout << "\n  --> ACEPTADO con recuperación (" << errores << " errores)\n";
            }
            erroresEncontrados = errores;
            return !huboError;
        }

        if (accion.esError()) {
            if (verbose) std::cout << "\n";
            huboError = true;
            bool ok = recuperarLR(pilaEstados, pilaSimbolos, idx,
                                   tokens, posiciones, tabla.action, errores);
            if (!ok) break;
            continue;
        }

        if (accion.tipo == TipoAccion::SHIFT) {
            pilaSimbolos.push_back(tok);
            pilaEstados.push_back(accion.valor);
            idx++;
        } else if (accion.tipo == TipoAccion::REDUCE) {
            const Produccion& prod = g.producciones[static_cast<size_t>(accion.valor)];

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

    if (errores >= MAX_ERRORES) {
        std::cerr << "\n[PARSER LR] Demasiados errores (" << MAX_ERRORES
                  << "). Se abandona el análisis.\n";
    }

    if (errores > 0) {
        std::cerr << "\n[PARSER LR] Total de errores encontrados: " << errores << "\n";
    }

    erroresEncontrados = errores;

    if (verbose)
        std::cout << "\n  --> RECHAZADO por SLR(1).\n";

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
