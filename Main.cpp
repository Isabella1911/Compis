/*
 * main.cpp  —  Pipeline completo: YALex + YAPar + LL(1)
 *
 * Compila desde la raiz del proyecto:
 *
 *   g++ -std=c++17 -O2 \
 *       main.cpp \
 *       lexer/YalexParser.cpp \
 *       parser/YaparParser.cpp parser/Grammar.cpp \
 *       parser/FirstFollow.cpp parser/LL1Table.cpp \
 *       -o compilador
 *
 * Uso:
 *   ./compilador <archivo.yal> <archivo.yapar> <archivo_entrada>
 *
 * Ejemplo:
 *   ./compilador input/lexer_complejo.yal \
 *                input/parser_complejo.yapar \
 *                input/entrada_complejo.txt
 */

#include "lexer/YalexParser.h"
#include "parser/YaparParser.h"
#include "parser/FirstFollow.h"
#include "parser/LL1Table.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <iomanip>

// ── Utilidades ────────────────────────────────────────────────────────────────

static std::string leerArchivo(const std::string& ruta) {
    std::ifstream f(ruta);
    if (!f.is_open())
        throw std::runtime_error("No se pudo abrir: " + ruta);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void separador(const std::string& titulo) {
    std::cout << "\n==================================================\n";
    std::cout << "  " << titulo << "\n";
    std::cout << "==================================================\n";
}

static void imprimirTokens(const ResultadoLexico& rl, const std::string& etiqueta) {
    std::cout << etiqueta << " (" << rl.tokens.size() << " tokens):\n";
    for (size_t i = 0; i < rl.tokens.size(); i++) {
        const Token& t = rl.tokens[i];
        std::cout << "  [" << i << "] " << t.id << "\t'" << t.valor << "'";
        if (t.id != "$")
            std::cout << "\tlinea " << rl.posiciones[i].linea
                      << ", col "  << rl.posiciones[i].columna;
        std::cout << "\n";
    }
}

static void imprimirGramatica(const Gramatica& g) {
    std::cout << "Simbolo inicial: " << g.simboloInicial << "\n\n";
    std::cout << "Terminales: ";
    bool primero = true;
    for (const auto& t : g.terminales) {
        if (!primero) std::cout << ", ";
        std::cout << t; primero = false;
    }
    std::cout << "\n\nNo terminales: ";
    primero = true;
    for (const auto& nt : g.noTerminales) {
        if (!primero) std::cout << ", ";
        std::cout << nt; primero = false;
    }
    std::cout << "\n\nProducciones:\n";
    for (size_t i = 0; i < g.producciones.size(); i++) {
        const Produccion& p = g.producciones[i];
        std::cout << "  " << i << ": " << p.izquierda << " ->";
        for (const auto& s : p.derecha) std::cout << " " << s;
        std::cout << "\n";
    }
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {

    if (argc != 4) {
        std::cerr << "\nUso:\n";
        std::cerr << "  " << argv[0]
                  << " <archivo.yal> <archivo.yapar> <archivo_entrada>\n\n";
        std::cerr << "Ejemplo:\n";
        std::cerr << "  " << argv[0]
                  << " input/lexer_complejo.yal"
                  << " input/parser_complejo.yapar"
                  << " input/entrada_complejo.txt\n\n";
        return 1;
    }

    const std::string rutaYal     = argv[1];
    const std::string rutaYapar   = argv[2];
    const std::string rutaEntrada = argv[3];

    // Crear carpeta output/ si no existe
    std::error_code ec;
    std::filesystem::create_directories("output", ec);
    if (ec) std::cerr << "Advertencia: no se pudo crear output/: " << ec.message() << "\n";

    std::cout << "\n╔══════════════════════════════════════════════════╗\n";
    std::cout << "║   COMPILADOR  |  YALex + YAPar + LL(1)          ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    std::cout << "  YAL:     " << rutaYal     << "\n";
    std::cout << "  YAPar:   " << rutaYapar   << "\n";
    std::cout << "  Entrada: " << rutaEntrada << "\n";

    // ══════════════════════════════════════════════════════════════
    //  FASE 1 — Generacion del analizador lexico (YALex)
    // ══════════════════════════════════════════════════════════════

    separador("FASE 1 — Lectura y expansion del .yal");

    ArchivoYalex yalex;
    try {
        yalex = parsear_yalex(rutaYal);
    } catch (const std::exception& e) {
        std::cerr << "Error en fase lexica (parsear_yalex): " << e.what() << "\n";
        return 1;
    }

    if (yalex.reglas.empty()) {
        std::cerr << "Error: no se encontraron reglas en " << rutaYal << "\n";
        return 1;
    }

    std::cout << "  Definiciones: " << yalex.definiciones_raw.size() << "\n";
    std::cout << "  Reglas:       " << yalex.reglas.size()           << "\n";
    std::cout << "  Rule:         " << yalex.nombre_regla            << "\n";

    fase2_expandir_y_unificar(yalex);
    std::cout << "  Expansion completada.\n";

    separador("FASE 1 — Construccion de automatas (AFN -> AFD -> min)");

    AFN afn = construir_afn_combinado(yalex.reglas);
    AFD afd = construir_afd_subconjuntos(afn);
    std::cout << "  AFD:     " << afd.estados.size() << " estados\n";

    AFD afd_min = minimizar_afd(afd);
    std::cout << "  AFD min: " << afd_min.estados.size() << " estados\n";

    // ══════════════════════════════════════════════════════════════
    //  FASE 2 — Analisis lexico del archivo de entrada
    // ══════════════════════════════════════════════════════════════

    separador("FASE 2 — Analisis lexico de la entrada");

    std::string textoEntrada;
    try {
        textoEntrada = leerArchivo(rutaEntrada);
    } catch (const std::exception& e) {
        std::cerr << "Error leyendo entrada: " << e.what() << "\n";
        return 1;
    }

    ResultadoLexico salidaLexer;
    try {
        salidaLexer = ejecutar_lexer(afd_min, yalex, textoEntrada);
    } catch (const std::exception& e) {
        std::cerr << "Error en analisis lexico: " << e.what() << "\n";
        return 1;
    }

    imprimirTokens(salidaLexer, "Tokens del lexer (antes de filtrar)");

    // ══════════════════════════════════════════════════════════════
    //  FASE 3 — Lectura del .yapar y filtrado de tokens
    // ══════════════════════════════════════════════════════════════

    separador("FASE 3 — Lectura del .yapar");

    YaparSpec spec;
    try {
        spec = leerYapar(rutaYapar);
    } catch (const std::exception& e) {
        std::cerr << "Error en YAPar: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Tokens declarados (%token): ";
    bool primero = true;
    for (const auto& t : spec.tokensDeclarados) {
        if (!primero) std::cout << ", ";
        std::cout << t; primero = false;
    }
    std::cout << "\n";

    std::cout << "Tokens ignorados (IGNORE):  ";
    primero = true;
    for (const auto& t : spec.tokensIgnorados) {
        if (!primero) std::cout << ", ";
        std::cout << t; primero = false;
    }
    if (spec.tokensIgnorados.empty()) std::cout << "(ninguno)";
    std::cout << "\n";

    try {
        validarTokensIgnorados(spec);
    } catch (const std::exception& e) {
        std::cerr << "Error validacion IGNORE: " << e.what() << "\n";
        return 1;
    }

    separador("FASE 3 — Filtrado de tokens ignorados");

    ResultadoLexico filtrada = filtrarTokensIgnorados(salidaLexer, spec.tokensIgnorados);
    imprimirTokens(filtrada, "Tokens para el parser (despues de filtrar)");

    separador("FASE 3 — Validacion YALex vs YAPar");

    validarTokensDeEntrada(filtrada.tokens, filtrada.posiciones, spec);
    std::cout << "  (errores de validacion aparecen arriba si los hay)\n";
    advertirTokensDeclaradosNoUsados(filtrada.tokens, spec);
    std::cout << "  (advertencias de tokens no usados aparecen arriba si las hay)\n";

    // ══════════════════════════════════════════════════════════════
    //  FASE 4 — Construccion de la gramatica y tabla LL(1)
    // ══════════════════════════════════════════════════════════════

    separador("FASE 4 — Gramatica");

    Gramatica gramatica;
    try {
        gramatica = construirGramatica(spec);
    } catch (const std::exception& e) {
        std::cerr << "Error construyendo gramatica: " << e.what() << "\n";
        return 1;
    }
    imprimirGramatica(gramatica);

    separador("FASE 4 — FIRST");
    MapaFirst first = calcularFirst(gramatica);
    imprimirFirst(first, gramatica);

    separador("FASE 4 — FOLLOW");
    MapaFollow follow = calcularFollow(gramatica, first);
    imprimirFollow(follow);

    separador("FASE 4 — Tabla LL(1)");
    ResultadoTablaLL1 resultado = construirTablaLL1(gramatica, first, follow);
    imprimirTablaLL1(resultado, gramatica);

    if (!resultado.esLL1()) {
        std::cout << "\n";
        imprimirConflictos(resultado.conflictos, gramatica);
    }


    // ══════════════════════════════════════════════════════════════
    //  FASE 5 — Parsing LL(1): simular la pila sobre el stream
    // ══════════════════════════════════════════════════════════════

    separador("FASE 5 — Parsing LL(1)");

    if (!resultado.esLL1()) {
        std::cout << "  La gramatica tiene conflictos LL(1).\n";
        std::cout << "  Se intentara parsear de todas formas usando la primera produccion en cada conflicto.\n\n";
    }

    {
        // Pila del parser: tope a la derecha
        std::vector<std::string> pila;
        pila.push_back("$");
        pila.push_back(gramatica.simboloInicial);

        size_t idx = 0;  // indice al token actual en filtrada
        bool exito = true;
        int pasos = 0;
        const int MAX_PASOS = 10000;

        std::cout << std::left
                  << std::setw(6)  << "Paso"
                  << std::setw(30) << "Tope pila"
                  << std::setw(20) << "Token actual"
                  << "Accion\n";
        std::cout << std::string(76, '-') << "\n";

        while (!pila.empty() && pasos < MAX_PASOS) {
            pasos++;
            const std::string& tope = pila.back();
            const std::string  tok  = (idx < filtrada.tokens.size())
                                      ? filtrada.tokens[idx].id : "$";

            // Mostrar estado actual (solo primeros 60 pasos para no inundar)
            if (pasos <= 60) {
                std::cout << std::left
                          << std::setw(6)  << pasos
                          << std::setw(30) << tope
                          << std::setw(20) << tok;
            }

            if (tope == "$") {
                if (tok == "$") {
                    if (pasos <= 60) std::cout << "ACEPTAR\n";
                    else if (pasos == 61) std::cout << "  (pasos restantes omitidos...)\n";
                    std::cout << "\n  --> ACEPTADO: el programa es sintacticamente correcto.\n";
                } else {
                    if (pasos <= 60) std::cout << "ERROR: entrada no consumida\n";
                    std::cerr << "\nERROR SINTACTICO: se esperaba fin de entrada pero se encontro '"
                              << tok << "' (linea " << filtrada.posiciones[idx].linea
                              << ", col " << filtrada.posiciones[idx].columna << ")\n";
                    exito = false;
                }
                break;

            } else if (gramatica.terminales.count(tope)) {
                // Terminal en tope de pila
                if (tope == tok) {
                    if (pasos <= 60) std::cout << "Consumir '" << tok << "'\n";
                    pila.pop_back();
                    idx++;
                } else {
                    if (pasos <= 60) std::cout << "ERROR\n";
                    std::cerr << "\nERROR SINTACTICO en linea "
                              << filtrada.posiciones[idx].linea
                              << ", col " << filtrada.posiciones[idx].columna
                              << ": se esperaba '" << tope
                              << "' pero se encontro '" << tok << "'\n";
                    exito = false;
                    break;
                }

            } else {
                // No terminal en tope de pila — buscar en tabla
                auto fila = resultado.tabla.find(tope);
                int prod_idx = -1;
                if (fila != resultado.tabla.end()) {
                    auto col = fila->second.find(tok);
                    if (col != fila->second.end())
                        prod_idx = col->second;
                }

                if (prod_idx == -1) {
                    if (pasos <= 60) std::cout << "ERROR\n";
                    std::cerr << "\nERROR SINTACTICO en linea "
                              << filtrada.posiciones[idx].linea
                              << ", col " << filtrada.posiciones[idx].columna
                              << ": no hay produccion para M[" << tope << ", " << tok << "]\n";
                    exito = false;
                    break;
                }

                const Produccion& prod = gramatica.producciones[prod_idx];
                if (pasos <= 60) {
                    std::cout << tope << " -> ";
                    for (const auto& s : prod.derecha) std::cout << s << " ";
                    std::cout << "\n";
                }

                pila.pop_back();
                // Apilar derecha de la produccion en orden inverso
                // (epsilon no se apila)
                if (!(prod.derecha.size() == 1 && prod.derecha[0] == "epsilon")) {
                    for (int k = (int)prod.derecha.size() - 1; k >= 0; k--)
                        pila.push_back(prod.derecha[k]);
                }
            }
        }

        if (pasos >= MAX_PASOS) {
            std::cerr << "\nERROR: se alcanzo el limite de pasos (" << MAX_PASOS
                      << "). Posible ciclo infinito en la gramatica.\n";
            exito = false;
        }

        separador("Resumen");

        std::cout << "  Reglas YALex:            " << yalex.reglas.size()              << "\n";
        std::cout << "  Estados AFD min:         " << afd_min.estados.size()           << "\n";
        std::cout << "  Tokens en entrada:       " << salidaLexer.tokens.size() - 1   << " (sin $)\n";
        std::cout << "  Tokens tras filtrado:    " << filtrada.tokens.size() - 1       << " (sin $)\n";
        std::cout << "  Producciones:            " << gramatica.producciones.size()    << "\n";
        {
            int celdas = 0;
            for (auto& fila : resultado.tabla)
                for (auto& col : fila.second)
                    if (col.second != -1) celdas++;
            std::cout << "  Celdas en tabla LL(1):   " << celdas                          << "\n";
        }
        std::cout << "  Conflictos LL(1):        " << resultado.conflictos.size()       << "\n";
        std::cout << "  Gramatica LL(1):         " << (resultado.esLL1() ? "SI" : "NO") << "\n";
        std::cout << "  Pasos de parsing:        " << pasos                             << "\n";
        std::cout << "  Resultado:               " << (exito ? "ACEPTADO" : "RECHAZADO") << "\n\n";

        return exito ? 0 : 3;
    }

}