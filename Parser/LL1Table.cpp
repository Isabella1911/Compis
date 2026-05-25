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

        for (const std::string& t : firstAlpha) {
            if (t == "epsilon") continue;

            int& celda = resultado.tabla[A][t];
            if (celda != -1 && celda != static_cast<int>(i)) {
                resultado.conflictos.push_back({A, t, celda, static_cast<int>(i)});
            } else {
                celda = static_cast<int>(i);
            }
        }

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

// ─── Tokens de sincronización para recuperación de errores ───────────────────

static bool esSincronizacion(const std::string& tok) {
    return tok == "SEMICOLON" || tok == "RBRACE" || tok == "$";
}

// ─── Calcular qué tokens se esperan dado el tope de pila actual ──────────────

static std::set<std::string> tokenesEsperados(
    const std::string& noTerminal,
    const ResultadoTablaLL1& tabla)
{
    std::set<std::string> esperados;
    auto it = tabla.tabla.find(noTerminal);
    if (it == tabla.tabla.end()) return esperados;
    for (const auto& [tok, prod] : it->second) {
        if (prod != -1) esperados.insert(tok);
    }
    return esperados;
}

// ─── Recuperación de errores por frase (phrase-level recovery) ───────────────
//
// Estrategia:
//   1. Reportar el error con línea, columna, qué se encontró y qué se esperaba.
//   2. Intentar ELIMINAR el token actual (si el siguiente token sí tiene producción).
//   3. Si no, intentar INSERTAR epsilon (vaciar el no-terminal de la pila).
//   4. Si no, sincronizar: descartar tokens hasta SEMICOLON / RBRACE / $
//      y limpiar la pila hasta encontrar un no-terminal que pueda continuar.
//
// Retorna true si se pudo reparar (continuar), false si se llegó a $.

static bool recuperarLL1(
    std::vector<std::string>& pila,
    size_t& idx,
    const std::vector<Token>& tokens,
    const std::vector<TokenPosicion>& posiciones,
    const ResultadoTablaLL1& tabla,
    const Gramatica& gramatica,
    int& erroresReportados)
{
    erroresReportados++;

    const std::string tokActual = (idx < tokens.size()) ? tokens[idx].id : "$";
    const TokenPosicion pos     = (idx < posiciones.size()) ? posiciones[idx] : TokenPosicion{-1,-1};
    const std::string& tope     = pila.back();

    // ── Armar mensaje de error ────────────────────────────────────────────────
    std::cerr << "\n[ERROR SINTÁCTICO #" << erroresReportados << "]";
    if (pos.linea > 0)
        std::cerr << " Línea " << pos.linea << ", col " << pos.columna;
    std::cerr << "\n";

    // Distinguir si el tope es terminal o no-terminal
    bool topeEsTerminal = (gramatica.noTerminales.find(tope) == gramatica.noTerminales.end()
                           && tope != "$");

    if (topeEsTerminal) {
        // El tope es un terminal que no coincide con el token actual
        std::cerr << "  Tipo:      Token inesperado\n";
        std::cerr << "  Encontró:  '" << tokActual << "'\n";
        std::cerr << "  Esperaba:  '" << tope << "'\n";
        std::cerr << "  Acción:    Se omite '" << tope << "' de la pila (inserción implícita)\n";
        // Reparación: eliminar el terminal de la pila (simula insertar el terminal esperado)
        pila.pop_back();
        return true;
    } else {
        // El tope es un no-terminal sin producción para el token actual
        std::set<std::string> esperados = tokenesEsperados(tope, tabla);
        std::cerr << "  Tipo:      No hay producción para M[" << tope << ", " << tokActual << "]\n";
        std::cerr << "  Encontró:  '" << tokActual << "'\n";
        if (!esperados.empty()) {
            std::cerr << "  Esperaba uno de: ";
            for (const auto& e : esperados) std::cerr << e << " ";
            std::cerr << "\n";
        }

        // ── Intento 1: eliminar token actual (si el siguiente sí tiene producción) ──
        if (!esSincronizacion(tokActual) && idx + 1 < tokens.size()) {
            const std::string& sigTok = tokens[idx + 1].id;
            auto it = tabla.tabla.find(tope);
            if (it != tabla.tabla.end()) {
                auto itP = it->second.find(sigTok);
                if (itP != it->second.end() && itP->second != -1) {
                    std::cerr << "  Acción:    Se descarta token '" << tokActual
                              << "' (eliminación)\n";
                    idx++;
                    return true;
                }
            }
        }

        // ── Intento 2: expandir con epsilon (vaciar no-terminal) ─────────────
        // Solo si el token actual está en FOLLOW del no-terminal
        // (significa que el no-terminal debería haber derivado vacío)
        auto itFollow = gramatica.noTerminales.find(tope); // solo para verificar que es NT
        if (itFollow != gramatica.noTerminales.end()) {
            // buscar si hay alguna producción con epsilon para este NT
            auto itTabla = tabla.tabla.find(tope);
            if (itTabla != tabla.tabla.end()) {
                for (const auto& [tok, prod] : itTabla->second) {
                    if (prod != -1) {
                        const Produccion& p = gramatica.producciones[static_cast<size_t>(prod)];
                        if (p.derecha.size() == 1 && p.derecha[0] == "epsilon") {
                            // Puede derivar epsilon — usarlo como reparación
                            std::cerr << "  Acción:    Se expande " << tope
                                      << " -> epsilon (inserción)\n";
                            pila.pop_back(); // vaciar el NT sin consumir token
                            return true;
                        }
                    }
                }
            }
        }

        // ── Intento 3: sincronización (pánico controlado) ─────────────────────
        std::cerr << "  Acción:    Sincronizando — descartando tokens hasta ';' '}' o fin\n";

        // Descartar tokens hasta encontrar uno de sincronización
        while (idx < tokens.size() && !esSincronizacion(tokens[idx].id)) {
            idx++;
        }

        if (idx < tokens.size() && tokens[idx].id == "SEMICOLON") {
            idx++; // consumir el ;
        }

        // Limpiar pila hasta llegar a un NT que pueda manejar el token actual
        // o hasta vaciar la pila
        const std::string tokSync = (idx < tokens.size()) ? tokens[idx].id : "$";
        while (pila.size() > 1) {
            const std::string& t2 = pila.back();
            if (t2 == "$") break;
            // Si es NT y tiene producción para tokSync, parar
            auto it = tabla.tabla.find(t2);
            if (it != tabla.tabla.end()) {
                auto itP = it->second.find(tokSync);
                if (itP != it->second.end() && itP->second != -1) break;
                // Si el NT puede derivar epsilon, también es un buen punto
                for (const auto& [tok, prod] : it->second) {
                    if (prod != -1) {
                        const Produccion& p = gramatica.producciones[static_cast<size_t>(prod)];
                        if (p.derecha.size() == 1 && p.derecha[0] == "epsilon") {
                            goto salir_limpieza;
                        }
                    }
                }
            }
            pila.pop_back();
        }
        salir_limpieza:;

        return (idx < tokens.size() && tokens[idx].id != "$") || pila.size() > 1;
    }
}

// ─── Evaluador LL(1) con recuperación de errores ─────────────────────────────

bool evaluarLL1ConRecuperacion(
    const ResultadoTablaLL1& tabla,
    const Gramatica& gramatica,
    const std::vector<Token>& tokens,
    const std::vector<TokenPosicion>& posiciones,
    int& pasos,
    bool verbose,
    int& erroresEncontrados)
{
    std::vector<std::string> pila;
    pila.push_back("$");
    pila.push_back(gramatica.simboloInicial);

    size_t idx            = 0;
    int    errores        = 0;
    bool   huboError      = false;
    pasos                 = 0;
    const int MAX_PASOS   = 10000;
    const int MAX_ERRORES = 20; // límite de errores antes de abandonar

    if (verbose) {
        std::cout << std::left
                  << std::setw(6)  << "Paso"
                  << std::setw(30) << "Tope pila"
                  << std::setw(20) << "Token actual"
                  << "Accion\n";
        std::cout << std::string(76, '-') << "\n";
    }

    while (!pila.empty() && pasos < MAX_PASOS && errores < MAX_ERRORES) {
        pasos++;
        const std::string& tope = pila.back();
        const std::string  tok  = (idx < tokens.size()) ? tokens[idx].id : "$";

        if (verbose) {
            const TokenPosicion pos = (idx < posiciones.size()) ? posiciones[idx] : TokenPosicion{-1,-1};
            std::cout << std::left
                      << std::setw(6)  << pasos
                      << std::setw(30) << tope
                      << std::setw(20) << tok;
        }

        // ── Caso 1: ambos son $ → aceptar ────────────────────────────────────
        if (tope == "$" && tok == "$") {
            if (verbose) std::cout << "ACEPTAR\n";
            if (verbose && !huboError)
                std::cout << "\n  --> ACEPTADO (con " << errores << " errores recuperados)\n";
            else if (verbose && huboError)
                std::cout << "\n  --> ACEPTADO con recuperación (" << errores << " errores)\n";
            erroresEncontrados = errores;
            return !huboError;
        }

        // ── Caso 2: tope es terminal ──────────────────────────────────────────
        if (gramatica.noTerminales.find(tope) == gramatica.noTerminales.end()) {
            if (tope == tok) {
                if (verbose) std::cout << "Consumir '" << tope << "'\n";
                pila.pop_back();
                idx++;
            } else {
                // Error: terminal esperado ≠ token actual
                if (verbose) std::cout << "\n";
                huboError = true;
                bool ok = recuperarLL1(pila, idx, tokens, posiciones, tabla, gramatica, errores);
                if (!ok) break;
            }
            continue;
        }

        // ── Caso 3: tope es no-terminal ───────────────────────────────────────
        auto itNT = tabla.tabla.find(tope);
        int prod = -1;
        if (itNT != tabla.tabla.end()) {
            auto itT = itNT->second.find(tok);
            if (itT != itNT->second.end()) prod = itT->second;
        }

        if (prod == -1) {
            // Error: no hay producción M[tope, tok]
            if (verbose) std::cout << "ERROR\n";
            huboError = true;
            bool ok = recuperarLL1(pila, idx, tokens, posiciones, tabla, gramatica, errores);
            if (!ok) break;
            continue;
        }

        // Producción encontrada: expandir
        const Produccion& p = gramatica.producciones[static_cast<size_t>(prod)];
        if (verbose) std::cout << produccionAString(p) << " \n";

        pila.pop_back();
        if (!(p.derecha.size() == 1 && p.derecha[0] == "epsilon")) {
            for (int k = static_cast<int>(p.derecha.size()) - 1; k >= 0; k--)
                pila.push_back(p.derecha[static_cast<size_t>(k)]);
        }
    }

    if (errores >= MAX_ERRORES) {
        std::cerr << "\n[PARSER LL(1)] Demasiados errores (" << MAX_ERRORES
                  << "). Se abandona el análisis.\n";
    }
    if (pasos >= MAX_PASOS) {
        std::cerr << "\n[PARSER LL(1)] Se alcanzó el límite de pasos (" << MAX_PASOS << ").\n";
    }

    if (errores > 0) {
        std::cerr << "\n[PARSER LL(1)] Total de errores encontrados: " << errores << "\n";
    }

    erroresEncontrados = errores;
    return false;
}
