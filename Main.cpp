/*
 * main.cpp  —  Pipeline completo: YALex + YAPar + LL(1) + LR(0) + SLR(1) + LALR(1)
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
#include <cstdio>

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
    // Conteo de errores recuperados por cada parser
    int               parseErroresLL1  = 0;
    int               parseErroresSLR  = 0;
    int               parseErroresLALR = 0;
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
// ║  Fase 5 — Parsing LL(1) con recuperación de errores        ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase5_parsing(const ResultadoLexico& filtrada,
                           const Gramatica& gramatica,
                           const ResultadoTablaLL1& tablaLL1,
                           int& pasos,
                           int& erroresEncontrados) {
    separador("FASE 5 — Parsing LL(1)");

    if (!tablaLL1.esLL1()) {
        std::cout << "  La gramatica tiene conflictos LL(1).\n";
        std::cout << "  Se intentara parsear usando la primera produccion en cada conflicto.\n\n";
    }

    return evaluarLL1ConRecuperacion(
        tablaLL1,
        gramatica,
        filtrada.tokens,
        filtrada.posiciones,
        pasos,
        true,
        erroresEncontrados
    );
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 6 — Automata LR(0)                                    ║
// ╚══════════════════════════════════════════════════════════════╝

static void fase6_lr0(const Gramatica& gramatica, AutomataLR0& lr0) {
    separador("FASE 6 — Automata LR(0)");

    lr0 = construirLR0(gramatica);

    std::cout << "  Estados LR(0): " << lr0.estados.size() << "\n\n";
    imprimirLR0(lr0);

    const std::string rutaDot = "output/lr0.dot";
    if (exportarLR0Dot(lr0, rutaDot)) {
        std::cout << "\n  [LR0-DOT] Autómata LR(0) exportado a: " << rutaDot << "\n";
    } else {
        std::cout << "\n  [LR0-DOT] No se pudo exportar a " << rutaDot << "\n";
    }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Fase 7 — Tabla SLR(1) y evaluacion                        ║
// ╚══════════════════════════════════════════════════════════════╝

static bool fase7_slr1(const AutomataLR0& lr0,
                        const MapaFollow& follow,
                        const ResultadoLexico& filtrada,
                        TablaSLR1& tablaSLR,
                        bool& parseExito,
                        int& erroresEncontrados) {
    separador("FASE 7 — Tabla SLR(1)");

    tablaSLR = construirSLR1(lr0, follow);
    imprimirTablaSLR1(tablaSLR);

    if (!tablaSLR.conflictos.empty()) {
        std::cout << "\n";
        imprimirConflictosSLR(tablaSLR.conflictos);
    }

    separador("FASE 7 — Evaluacion SLR(1)");
    parseExito = evaluarSLR1(tablaSLR, filtrada.tokens, filtrada.posiciones, true,
                              erroresEncontrados);
    
    if (parseExito && erroresEncontrados == 0)
        std::cout << "\n  --> ACEPTADO por SLR(1).\n";
    else if (erroresEncontrados > 0)
        std::cout << "\n  --> ACEPTADO CON ERRORES (" << erroresEncontrados << ") por SLR(1).\n";
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
                         bool& parseExito,
                         int& erroresEncontrados) {
    separador("FASE 8 — Tabla LALR(1)");

    tablaLALR = construirLALR1(lr0, first);
    imprimirTablaLALR1(tablaLALR);

    separador("FASE 8 — Evaluacion LALR(1)");
    parseExito = evaluarLALR1(tablaLALR, filtrada.tokens, filtrada.posiciones, true,
                               erroresEncontrados);

    if (parseExito && erroresEncontrados == 0)
        std::cout << "\n  --> ACEPTADO por LALR(1).\n";
    else if (erroresEncontrados > 0)
        std::cout << "\n  --> ACEPTADO CON ERRORES (" << erroresEncontrados << ") por LALR(1).\n";
    else
        std::cout << "\n  --> RECHAZADO por LALR(1).\n";

    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Exportación de tablas a JSON                               ║
// ╚══════════════════════════════════════════════════════════════╝

static std::string escaparJSON(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    r += buf;
                } else {
                    r += c;
                }
        }
    }
    return r;
}

static std::string accionSLRAJson(const AccionSLR& a) {
    switch (a.tipo) {
        case TipoAccion::SHIFT:  return "s" + std::to_string(a.valor);
        case TipoAccion::REDUCE: return "r" + std::to_string(a.valor);
        case TipoAccion::ACCEPT: return "acc";
        case TipoAccion::ERROR:  return "";
    }
    return "";
}

static void escribirTablaSLREnJSON(std::ostream& out,
                                    const std::string& clave,
                                    const TablaSLR1& tabla,
                                    bool primero) {
    if (!primero) out << ",\n";
    const Gramatica& g = tabla.gramatica;

    std::vector<std::string> terms(g.terminales.begin(), g.terminales.end());
    terms.push_back("$");
    std::vector<std::string> nterms;
    for (const auto& nt : g.noTerminales) {
        if (nt != g.simboloInicial) nterms.push_back(nt);
    }

    out << "  \"" << clave << "\": {\n";

    out << "    \"terminales\": [";
    for (size_t i = 0; i < terms.size(); i++) {
        if (i) out << ", ";
        out << "\"" << escaparJSON(terms[i]) << "\"";
    }
    out << "],\n";

    out << "    \"no_terminales\": [";
    for (size_t i = 0; i < nterms.size(); i++) {
        if (i) out << ", ";
        out << "\"" << escaparJSON(nterms[i]) << "\"";
    }
    out << "],\n";

    out << "    \"action\": [";
    for (size_t i = 0; i < tabla.action.size(); i++) {
        if (i) out << ", ";
        out << "{";
        bool first = true;
        for (const auto& [t, a] : tabla.action[i]) {
            std::string repr = accionSLRAJson(a);
            if (repr.empty()) continue;
            if (!first) out << ", ";
            out << "\"" << escaparJSON(t) << "\": \"" << repr << "\"";
            first = false;
        }
        out << "}";
    }
    out << "],\n";

    out << "    \"goto\": [";
    for (size_t i = 0; i < tabla.goto_.size(); i++) {
        if (i) out << ", ";
        out << "{";
        bool first = true;
        for (const auto& [nt, dest] : tabla.goto_[i]) {
            if (!first) out << ", ";
            out << "\"" << escaparJSON(nt) << "\": " << dest;
            first = false;
        }
        out << "}";
    }
    out << "],\n";

    out << "    \"conflictos\": [";
    for (size_t i = 0; i < tabla.conflictos.size(); i++) {
        if (i) out << ", ";
        const auto& c = tabla.conflictos[i];
        out << "{"
            << "\"estado\": " << c.estado
            << ", \"terminal\": \"" << escaparJSON(c.terminal) << "\""
            << ", \"descripcion\": \"" << escaparJSON(c.descripcion) << "\""
            << "}";
    }
    out << "]\n";

    out << "  }";
}

static bool escribirTablasJSON(const ResultadoPipeline& r,
                                const std::string& ruta) {
    std::ofstream out(ruta);
    if (!out.is_open()) return false;

    out << "{\n";

    out << "  \"first\": {";
    {
        bool primero = true;
        for (const auto& [nt, conj] : r.first) {
            if (!primero) out << ",";
            out << "\n    \"" << escaparJSON(nt) << "\": [";
            bool f = true;
            for (const auto& t : conj) {
                if (!f) out << ", ";
                out << "\"" << escaparJSON(t) << "\"";
                f = false;
            }
            out << "]";
            primero = false;
        }
    }
    out << "\n  },\n";

    out << "  \"follow\": {";
    {
        bool primero = true;
        for (const auto& [nt, conj] : r.follow) {
            if (!primero) out << ",";
            out << "\n    \"" << escaparJSON(nt) << "\": [";
            bool f = true;
            for (const auto& t : conj) {
                if (!f) out << ", ";
                out << "\"" << escaparJSON(t) << "\"";
                f = false;
            }
            out << "]";
            primero = false;
        }
    }
    out << "\n  },\n";

    {
        std::vector<std::string> terms(r.gramatica.terminales.begin(),
                                        r.gramatica.terminales.end());
        terms.push_back("$");
        out << "  \"ll1\": {\n";
        out << "    \"terminales\": [";
        for (size_t i = 0; i < terms.size(); i++) {
            if (i) out << ", ";
            out << "\"" << escaparJSON(terms[i]) << "\"";
        }
        out << "],\n";

        out << "    \"filas\": {";
        bool primero = true;
        for (const auto& [nt, fila] : r.tablaLL1.tabla) {
            if (!primero) out << ",";
            out << "\n      \"" << escaparJSON(nt) << "\": {";
            bool f = true;
            for (const auto& [t, prod] : fila) {
                if (prod < 0) continue;
                if (!f) out << ", ";
                out << "\"" << escaparJSON(t) << "\": " << prod;
                f = false;
            }
            out << "}";
            primero = false;
        }
        out << "\n    }\n";
        out << "  },\n";
    }

    escribirTablaSLREnJSON(out, "slr1",  r.slr1,  true);
    escribirTablaSLREnJSON(out, "lalr1", r.lalr1, false);
    out << "\n";

    out << "}\n";
    return true;
}

// ╔══════════════════════════════════════════════════════════════╗
// ║  Resumen final                                              ║
// ╚══════════════════════════════════════════════════════════════╝

static std::string resultadoParser(bool exito, int errores) {
    if (exito && errores == 0) return "ACEPTADO";
    if (errores > 0)           return "ACEPTADO CON ERRORES (" + std::to_string(errores) + ")";
    return "RECHAZADO";
}

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

    std::cout << "  Reglas YALex:              " << r.yalex.reglas.size()            << "\n";
    std::cout << "  Estados AFD min:           " << r.afd_min.estados.size()         << "\n";
    std::cout << "  Tokens en entrada:         " << r.salidaLexer.tokens.size() - 1 << " (sin $)\n";
    std::cout << "  Tokens tras filtrado:      " << r.filtrada.tokens.size() - 1     << " (sin $)\n";
    std::cout << "  Identificadores unicos:    " << r.tablaSimbolos.size()           << "\n";
    std::cout << "  Producciones:              " << r.gramatica.producciones.size()  << "\n";
    std::cout << "\n";
    std::cout << "  LL(1):\n";
    std::cout << "    Celdas:                  " << celdasLL1                                       << "\n";
    std::cout << "    Conflictos:              " << r.tablaLL1.conflictos.size()                   << "\n";
    std::cout << "    Es LL(1):                " << (r.tablaLL1.esLL1() ? "SI" : "NO")            << "\n";
    std::cout << "    Errores recuperados:     " << r.parseErroresLL1                              << "\n";
    std::cout << "    Resultado:               " << resultadoParser(r.parseExitoLL1,  r.parseErroresLL1)  << "\n";
    std::cout << "\n";
    std::cout << "  LR(0):\n";
    std::cout << "    Estados:                 " << r.lr0.estados.size()                           << "\n";
    std::cout << "\n";
    std::cout << "  SLR(1):\n";
    std::cout << "    Celdas ACTION:           " << celdasSLR                                      << "\n";
    std::cout << "    Conflictos:              " << r.slr1.conflictos.size()                       << "\n";
    std::cout << "    Es SLR(1):               " << (r.slr1.esSLR1() ? "SI" : "NO")              << "\n";
    std::cout << "    Errores recuperados:     " << r.parseErroresSLR                             << "\n";
    std::cout << "    Resultado:               " << resultadoParser(r.parseExitoSLR1, r.parseErroresSLR) << "\n";
    std::cout << "\n";
    std::cout << "  LALR(1):\n";
    std::cout << "    Celdas ACTION:           " << celdasLALR                                     << "\n";
    std::cout << "    Conflictos:              " << r.lalr1.conflictos.size()                      << "\n";
    std::cout << "    Es LALR(1):              " << (esLALR1(r.lalr1) ? "SI" : "NO")             << "\n";
    std::cout << "    Errores recuperados:     " << r.parseErroresLALR                            << "\n";
    std::cout << "    Resultado:               " << resultadoParser(r.parseExitoLALR, r.parseErroresLALR) << "\n";
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

    if (!fase1_lexer(rutaYal, r.yalex, r.afd_min))            return 1;
    if (!fase2_analisisLexico(rutaEntrada, r.afd_min,
                               r.yalex, r.salidaLexer))        return 1;
    if (!fase3_yapar(rutaYapar, r.salidaLexer,
                     spec, r.filtrada, r.tablaSimbolos))        return 1;
    if (!fase4_gramatica(spec, r.gramatica,
                          r.first, r.follow, r.tablaLL1))       return 1;

    r.parseExitoLL1 = fase5_parsing(r.filtrada, r.gramatica,
                                     r.tablaLL1, r.parseSteps,
                                     r.parseErroresLL1);

    fase6_lr0(r.gramatica, r.lr0);
    fase7_slr1(r.lr0, r.follow, r.filtrada, r.slr1,
               r.parseExitoSLR1, r.parseErroresSLR);
    fase8_lalr1(r.lr0, r.first, r.filtrada, r.lalr1,
                r.parseExitoLALR, r.parseErroresLALR);

    imprimirResumen(r);

    const std::string rutaJson = "output/tablas.json";
    if (escribirTablasJSON(r, rutaJson)) {
        std::cout << "  [TABLAS-JSON] Tablas exportadas a: " << rutaJson << "\n";
    } else {
        std::cout << "  [TABLAS-JSON] No se pudo exportar a " << rutaJson << "\n";
    }


    // Aceptado limpio: ningún parser tuvo errores
    bool aceptadoLimpio = (r.parseExitoLL1 && r.parseErroresLL1 == 0) ||
                          (r.parseExitoSLR1 && r.parseErroresSLR == 0) ||
                          (r.parseExitoLALR && r.parseErroresLALR == 0);

    // Aceptado con recuperación: al menos uno llegó a acc aunque con errores
    bool aceptadoConErrores = (r.parseErroresLL1 > 0) ||
                              (r.parseErroresSLR > 0) ||
                              (r.parseErroresLALR > 0);

    if (aceptadoLimpio)       return 0;  // ACEPTADO
    if (aceptadoConErrores)   return 1;  // ACEPTADO CON ERRORES (código distinto de 3)
    return 3;                             // RECHAZADO
}
