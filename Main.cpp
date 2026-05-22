/*
 * main.cpp  —  Pipeline completo: YALex + YAPar + LL(1) + LR(0) + SLR(1) + LALR(1)
 *
 * Compila desde la raiz del proyecto:
 *
 *   g++ -std=c++17 -O2 -DCOMPILAR_CON_ORQUESTADOR \
 *       main.cpp \
 *       lexer/YalexParser.cpp \
 *       parser/YaparParser.cpp parser/Grammar.cpp \
 *       parser/FirstFollow.cpp parser/LL1Table.cpp \
 *       parser/LR0.cpp parser/SLR1.cpp parser/LALR1.cpp \
 *       -o compilador
 *
 * Uso:
 *   ./compilador                                      (menu interactivo)
 *   ./compilador <archivo.yal> <archivo.yapar> <entrada>
 */

#include "lexer/YalexParser.h"
#include "parser/YaparParser.h"
#include "parser/FirstFollow.h"
#include "parser/LL1Table.h"
#include "parser/LR0.h"
#include "parser/SLR1.h"
#include "parser/LALR1.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <iomanip>
#include <map>
#include <vector>

// ╔══════════════════════════════════════════════════════════════╗
// ║  Estructuras                                                ║
// ╚══════════════════════════════════════════════════════════════╝

struct EntradaSimbolo {
    std::string nombre;
    int         apariciones = 0;
};

using TablaSimbolos = std::map<std::string, EntradaSimbolo>;

struct ConjuntoArchivos {
    std::string nombre;
    std::string yal;
    std::string yapar;
    std::string entrada;
};

struct ResultadoPipeline {
    ArchivoYalex      yalex;
    AFD               afd_min;
    ResultadoLexico   salidaLexer;
    ResultadoLexico   filtrada;
    TablaSimbolos     tablaSimbolos;
    Gramatica         gramatica;
    MapaFirst         first;
    MapaFollow        follow;
    ResultadoTablaLL1 tablaLL1;
    AutomataLR0       lr0;
    TablaSLR1         slr1;
    TablaLALR1        lalr1;
    bool              parseExitoLL1  = false;
    bool              parseExitoSLR1 = false;
    bool              parseExitoLALR = false;
    int               parseSteps     = 0;
};

// ╔══════════════════════════════════════════════════════════════╗
// ║  Utilidades de impresion                                    ║
// ╚══════════════════════════════════════════════════════════════╝

static void separador(const std::string& titulo) {
    std::cout << "\n==================================================\n";
    std::cout << "  " << titulo << "\n";
    std::cout << "==================================================\n";
}

static std::string leerArchivo(const std::string& ruta) {
    std::ifstream f(ruta);
    if (!f.is_open())
        throw std::runtime_error("No se pudo abrir: " + ruta);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
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

static void imprimirTablaSimbolos(const TablaSimbolos& tabla) {
    if (tabla.empty()) {
        std::cout << "  (no se encontraron identificadores)\n";
        return;
    }
    std::cout << std::left
              << std::setw(20) << "Identificador"
              << "Apariciones\n";
    std::cout << std::string(32, '-') << "\n";
    for (const auto& [nombre, entrada] : tabla)
        std::cout << std::left << std::setw(20) << entrada.nombre
                  << entrada.apariciones << "\n";
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 1 — Generacion del analizador lexico                  ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase1_lexer(const std::string& rutaYal,
                        ArchivoYalex& yalex, AFD& afd_min) {
    separador("FASE 1 — Lectura y expansion del .yal");

    try {
        yalex = parsear_yalex(rutaYal);
    } catch (const std::exception& e) {
        std::cerr << "Error en fase lexica: " << e.what() << "\n";
        return false;
    }

    if (yalex.reglas.empty()) {
        std::cerr << "Error: no se encontraron reglas en " << rutaYal << "\n";
        return false;
    }

    std::cout << "  Definiciones: " << yalex.definiciones_raw.size() << "\n";
    std::cout << "  Reglas:       " << yalex.reglas.size()           << "\n";
    std::cout << "  Rule:         " << yalex.nombre_regla            << "\n";

    fase2_expandir_y_unificar(yalex);
    std::cout << "  Expansion completada.\n";

    separador("FASE 1 — Construccion de automatas (AFN -> AFD -> min)");

    AFN afn = construir_afn_combinado(yalex.reglas);
    AFD afd = construir_afd_subconjuntos(afn);
    afd_min = minimizar_afd(afd);

    std::cout << "  AFD:     " << afd.estados.size()     << " estados\n";
    std::cout << "  AFD min: " << afd_min.estados.size() << " estados\n";
    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 2 — Analisis lexico de la entrada                     ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase2_analisisLexico(const std::string& rutaEntrada,
                                  AFD& afd_min, const ArchivoYalex& yalex,
                                  ResultadoLexico& salidaLexer) {
    separador("FASE 2 — Analisis lexico de la entrada");

    std::string texto;
    try {
        texto = leerArchivo(rutaEntrada);
    } catch (const std::exception& e) {
        std::cerr << "Error leyendo entrada: " << e.what() << "\n";
        return false;
    }

    try {
        salidaLexer = ejecutar_lexer(afd_min, yalex, texto);
    } catch (const std::exception& e) {
        std::cerr << "Error en analisis lexico: " << e.what() << "\n";
        return false;
    }

    imprimirTokens(salidaLexer, "Tokens del lexer (antes de filtrar)");
    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 3 — YAPar, filtrado, validacion y tabla de simbolos   ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase3_yapar(const std::string& rutaYapar,
                         const ResultadoLexico& salidaLexer,
                         YaparSpec& spec,
                         ResultadoLexico& filtrada,
                         TablaSimbolos& tablaSimbolos) {
    separador("FASE 3 — Lectura del .yapar y filtrado");

    try {
        spec = leerYapar(rutaYapar);
    } catch (const std::exception& e) {
        std::cerr << "Error en YAPar: " << e.what() << "\n";
        return false;
    }

    try {
        validarTokensIgnorados(spec);
    } catch (const std::exception& e) {
        std::cerr << "Error de validacion IGNORE: " << e.what() << "\n";
        return false;
    }

    filtrada = filtrarTokensIgnorados(salidaLexer, spec.tokensIgnorados);
    imprimirTokens(filtrada, "Tokens para el parser (despues de filtrar)");

    validarTokensDeEntrada(filtrada.tokens, filtrada.posiciones, spec);
    advertirTokensDeclaradosNoUsados(filtrada.tokens, spec);

    separador("FASE 3 — Tabla de simbolos");
    for (size_t i = 0; i < filtrada.tokens.size(); i++) {
        const Token& tok = filtrada.tokens[i];
        if (tok.id == "ID" || tok.id == "id") {
            tablaSimbolos[tok.valor].nombre = tok.valor;
            tablaSimbolos[tok.valor].apariciones++;
        }
    }
    imprimirTablaSimbolos(tablaSimbolos);

    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 4 — Gramatica, FIRST, FOLLOW, tabla LL(1)             ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase4_gramatica(const YaparSpec& spec,
                             Gramatica& gramatica,
                             MapaFirst& first,
                             MapaFollow& follow,
                             ResultadoTablaLL1& tablaLL1) {
    separador("FASE 4 — Gramatica");

    try {
        gramatica = construirGramatica(spec);
    } catch (const std::exception& e) {
        std::cerr << "Error construyendo gramatica: " << e.what() << "\n";
        return false;
    }
    imprimirGramatica(gramatica);

    separador("FASE 4 — FIRST");
    first = calcularFirst(gramatica);
    imprimirFirst(first, gramatica);

    separador("FASE 4 — FOLLOW");
    follow = calcularFollow(gramatica, first);
    imprimirFollow(follow);

    separador("FASE 4 — Tabla LL(1)");
    tablaLL1 = construirTablaLL1(gramatica, first, follow);
    imprimirTablaLL1(tablaLL1, gramatica);

    if (!tablaLL1.esLL1()) {
        std::cout << "\n";
        imprimirConflictos(tablaLL1.conflictos, gramatica);
    }

    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 5 — Parsing LL(1)                                     ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase5_parsing(const ResultadoLexico& filtrada,
                           const Gramatica& gramatica,
                           const ResultadoTablaLL1& tablaLL1,
                           int& pasos) {
    separador("FASE 5 — Parsing LL(1)");

    if (!tablaLL1.esLL1()) {
        std::cout << "  La gramatica tiene conflictos LL(1).\n";
        std::cout << "  Se intentara parsear usando la primera produccion en cada conflicto.\n\n";
    }

    std::vector<std::string> pila;
    pila.push_back("$");
    pila.push_back(gramatica.simboloInicial);

    size_t idx   = 0;
    bool   exito = true;
    pasos        = 0;
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

        if (pasos <= 60) {
            std::cout << std::left
                      << std::setw(6)  << pasos
                      << std::setw(30) << tope
                      << std::setw(20) << tok;
        } else if (pasos == 61) {
            std::cout << "  (pasos restantes omitidos...)\n";
        }

        if (tope == "$") {
            if (tok == "$") {
                if (pasos <= 60) std::cout << "ACEPTAR\n";
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
            auto fila = tablaLL1.tabla.find(tope);
            int prod_idx = -1;
            if (fila != tablaLL1.tabla.end()) {
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
            if (!(prod.derecha.size() == 1 && prod.derecha[0] == "epsilon"))
                for (int k = (int)prod.derecha.size() - 1; k >= 0; k--)
                    pila.push_back(prod.derecha[k]);
        }
    }

    if (pasos >= MAX_PASOS) {
        std::cerr << "\nERROR: se alcanzo el limite de pasos (" << MAX_PASOS << ").\n";
        exito = false;
    }

    return exito;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 6 — Automata LR(0)                                    ║
// ╚══════════════════════════════════════════════════════════════╝

static void fase6_lr0(const Gramatica& gramatica, AutomataLR0& lr0) {
    separador("FASE 6 — Automata LR(0)");

    lr0 = construirLR0(gramatica);

    std::cout << "  Estados LR(0): " << lr0.estados.size() << "\n\n";
    imprimirLR0(lr0);
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 7 — Tabla SLR(1) y evaluacion                        ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase7_slr1(const AutomataLR0& lr0,
                        const MapaFollow& follow,
                        const ResultadoLexico& filtrada,
                        TablaSLR1& tablaSLR,
                        bool& parseExito) {
    separador("FASE 7 — Tabla SLR(1)");

    tablaSLR = construirSLR1(lr0, follow);
    imprimirTablaSLR1(tablaSLR);

    if (!tablaSLR.conflictos.empty()) {
        std::cout << "\n";
        imprimirConflictosSLR(tablaSLR.conflictos);
    }

    separador("FASE 7 — Evaluacion SLR(1)");
    parseExito = evaluarSLR1(tablaSLR, filtrada.tokens, filtrada.posiciones, true);

    if (parseExito)
        std::cout << "\n  --> ACEPTADO por SLR(1).\n";
    else
        std::cout << "\n  --> RECHAZADO por SLR(1).\n";

    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 8 — Tabla LALR(1) y evaluacion                       ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase8_lalr1(const AutomataLR0& lr0,
                         const MapaFirst& first,
                         const ResultadoLexico& filtrada,
                         TablaLALR1& tablaLALR,
                         bool& parseExito) {
    separador("FASE 8 — Tabla LALR(1)");

    tablaLALR = construirLALR1(lr0, first);
    imprimirTablaLALR1(tablaLALR);

    separador("FASE 8 — Evaluacion LALR(1)");
    parseExito = evaluarLALR1(tablaLALR, filtrada.tokens, filtrada.posiciones, true);

    if (parseExito)
        std::cout << "\n  --> ACEPTADO por LALR(1).\n";
    else
        std::cout << "\n  --> RECHAZADO por LALR(1).\n";

    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Resumen final                                              ║
// ╚══════════════════════════════════════════════════════════════╝

static void imprimirResumen(const ResultadoPipeline& r) {
    separador("Resumen");

    int celdasLL1 = 0;
    for (auto& fila : r.tablaLL1.tabla)
        for (auto& col : fila.second)
            if (col.second != -1) celdasLL1++;

    int celdasSLR = 0;
    for (auto& fila : r.slr1.action)
        for (auto& col : fila)
            if (!col.second.esError()) celdasSLR++;

    int celdasLALR = 0;
    for (auto& fila : r.lalr1.action)
        for (auto& col : fila)
            if (!col.second.esError()) celdasLALR++;

    std::cout << "  Reglas YALex:              " << r.yalex.reglas.size()              << "\n";
    std::cout << "  Estados AFD min:           " << r.afd_min.estados.size()           << "\n";
    std::cout << "  Tokens en entrada:         " << r.salidaLexer.tokens.size() - 1   << " (sin $)\n";
    std::cout << "  Tokens tras filtrado:      " << r.filtrada.tokens.size() - 1       << " (sin $)\n";
    std::cout << "  Identificadores unicos:    " << r.tablaSimbolos.size()             << "\n";
    std::cout << "  Producciones:              " << r.gramatica.producciones.size()    << "\n";
    std::cout << "\n";
    std::cout << "  LL(1):\n";
    std::cout << "    Celdas:                  " << celdasLL1                           << "\n";
    std::cout << "    Conflictos:              " << r.tablaLL1.conflictos.size()        << "\n";
    std::cout << "    Es LL(1):                " << (r.tablaLL1.esLL1() ? "SI" : "NO") << "\n";
    std::cout << "    Resultado:               " << (r.parseExitoLL1 ? "ACEPTADO" : "RECHAZADO") << "\n";
    std::cout << "\n";
    std::cout << "  LR(0):\n";
    std::cout << "    Estados:                 " << r.lr0.estados.size()               << "\n";
    std::cout << "\n";
    std::cout << "  SLR(1):\n";
    std::cout << "    Celdas ACTION:           " << celdasSLR                           << "\n";
    std::cout << "    Conflictos:              " << r.slr1.conflictos.size()            << "\n";
    std::cout << "    Es SLR(1):               " << (r.slr1.esSLR1() ? "SI" : "NO")   << "\n";
    std::cout << "    Resultado:               " << (r.parseExitoSLR1 ? "ACEPTADO" : "RECHAZADO") << "\n";
    std::cout << "\n";
    std::cout << "  LALR(1):\n";
    std::cout << "    Celdas ACTION:           " << celdasLALR                          << "\n";
    std::cout << "    Conflictos:              " << r.lalr1.conflictos.size()           << "\n";
    std::cout << "    Es LALR(1):              " << (r.lalr1.esSLR1() ? "SI" : "NO")  << "\n";
    std::cout << "    Resultado:               " << (r.parseExitoLALR ? "ACEPTADO" : "RECHAZADO") << "\n";
    std::cout << "\n";
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Menu de seleccion de archivos                              ║
// ╚══════════════════════════════════════════════════════════════╝

static bool seleccionarArchivos(int argc, char* argv[],
                                 std::string& yal,
                                 std::string& yapar,
                                 std::string& entrada) {
    const std::vector<ConjuntoArchivos> conjuntos = {
        {
            "Lenguaje complejo (if/while/return/expresiones)",
            "input/lexer_complejo.yal",
            "input/parser_complejo.yapar",
            "input/entrada_complejo.txt"
        },
        {
            "Lenguaje complejo — caso de prueba alternativo",
            "input/lexer_complejo.yal",
            "input/parser_complejo.yapar",
            "input/Test_complejo.txt"
        },
        {
            "Expresiones aritmeticas simples (ejemplo.yal)",
            "input/ejemplo.yal",
            "input/parser_ejemplo.yapar",
            "input/entrada_ejemplo.txt"
        }
    };

    if (argc == 4) {
        yal     = argv[1];
        yapar   = argv[2];
        entrada = argv[3];
        return true;
    }

    if (argc != 1) {
        std::cerr << "\nUso:\n";
        std::cerr << "  " << argv[0] << "                                    (menu interactivo)\n";
        std::cerr << "  " << argv[0] << " <archivo.yal> <archivo.yapar> <entrada>\n\n";
        return false;
    }

    std::cout << "\n╔══════════════════════════════════════════════════╗\n";
    std::cout << "║   COMPILADOR  |  Seleccion de archivos           ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";
    std::cout << "Selecciona un conjunto de archivos:\n\n";

    for (size_t i = 0; i < conjuntos.size(); i++) {
        std::cout << "  [" << (i + 1) << "] " << conjuntos[i].nombre << "\n";
        std::cout << "      YAL:     " << conjuntos[i].yal     << "\n";
        std::cout << "      YAPar:   " << conjuntos[i].yapar   << "\n";
        std::cout << "      Entrada: " << conjuntos[i].entrada << "\n\n";
    }
    std::cout << "  [0] Ingresar rutas manualmente\n\n";
    std::cout << "Opcion: ";

    int opcion = -1;
    std::cin >> opcion;

    if (opcion >= 1 && opcion <= (int)conjuntos.size()) {
        yal     = conjuntos[opcion - 1].yal;
        yapar   = conjuntos[opcion - 1].yapar;
        entrada = conjuntos[opcion - 1].entrada;
    } else if (opcion == 0) {
        std::cout << "\nRuta del archivo .yal:     "; std::cin >> yal;
        std::cout << "Ruta del archivo .yapar:   "; std::cin >> yapar;
        std::cout << "Ruta del archivo entrada:  "; std::cin >> entrada;
    } else {
        std::cerr << "Opcion invalida.\n";
        return false;
    }

    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  main                                                       ║
// ╚══════════════════════════════════════════════════════════════╝

int main(int argc, char* argv[]) {

    std::string rutaYal, rutaYapar, rutaEntrada;
    if (!seleccionarArchivos(argc, argv, rutaYal, rutaYapar, rutaEntrada))
        return 1;

    std::error_code ec;
    std::filesystem::create_directories("output", ec);

    std::cout << "\n╔══════════════════════════════════════════════════╗\n";
    std::cout << "║   COMPILADOR  |  YALex + YAPar + LL(1) + LR/SLR/LALR ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    std::cout << "  YAL:     " << rutaYal     << "\n";
    std::cout << "  YAPar:   " << rutaYapar   << "\n";
    std::cout << "  Entrada: " << rutaEntrada << "\n";

    ResultadoPipeline r;
    YaparSpec spec;

    // Fases existentes (lexer + LL(1))
    if (!fase1_lexer(rutaYal, r.yalex, r.afd_min))            return 1;
    if (!fase2_analisisLexico(rutaEntrada, r.afd_min,
                               r.yalex, r.salidaLexer))        return 1;
    if (!fase3_yapar(rutaYapar, r.salidaLexer,
                     spec, r.filtrada, r.tablaSimbolos))        return 1;
    if (!fase4_gramatica(spec, r.gramatica,
                          r.first, r.follow, r.tablaLL1))       return 1;

    r.parseExitoLL1 = fase5_parsing(r.filtrada, r.gramatica,
                                     r.tablaLL1, r.parseSteps);

    // Nuevas fases: LR(0) + SLR(1) + LALR(1)
    fase6_lr0(r.gramatica, r.lr0);
    fase7_slr1(r.lr0, r.follow, r.filtrada, r.slr1, r.parseExitoSLR1);
    fase8_lalr1(r.lr0, r.first, r.filtrada, r.lalr1, r.parseExitoLALR);

    imprimirResumen(r);

    // Exitoso si al menos uno de los parsers acepta
    bool exito = r.parseExitoLL1 || r.parseExitoSLR1 || r.parseExitoLALR;
    return exito ? 0 : 3;
}