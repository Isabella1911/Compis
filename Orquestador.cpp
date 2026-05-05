/*
 * orquestador.cpp
 *
 * Pipeline completo: Lexer generado -> YAPar -> FIRST/FOLLOW -> Tabla LL(1)
 *
 * Compilar junto al lexer generado:
 *
 *   g++ -std=c++17 -DCOMPILAR_CON_ORQUESTADOR \
 *       orquestador.cpp <lexer_generado>.cpp \
 *       YaparParser.cpp Grammar.cpp FirstFollow.cpp LL1Table.cpp \
 *       -o parser
 *
 * Uso:
 *   ./parser <archivo.yapar> <archivo_entrada>
 *
 * Ejemplo:
 *   ./parser parser.yapar entrada.txt
 */

#include "YaparParser.h"
#include "FirstFollow.h"
#include "LL1Table.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// Declaración de la función que expone el lexer generado.
// Su definición viene del <lexer_generado>.cpp compilado junto a este archivo.
ResultadoLexico analizar_tokens(const std::string& entrada);

// ─── utilidades de impresión ──────────────────────────────────────────────────

static void imprimirSeparador(const std::string& titulo) {
    std::cout << "\n";
    std::cout << "══════════════════════════════════════════════════\n";
    std::cout << "  " << titulo << "\n";
    std::cout << "══════════════════════════════════════════════════\n";
}

static void imprimirTokens(const ResultadoLexico& rl, const std::string& etiqueta) {
    std::cout << etiqueta << " (" << rl.tokens.size() << " tokens):\n";
    for (size_t i = 0; i < rl.tokens.size(); i++) {
        const Token& t = rl.tokens[i];
        std::cout << "  [" << i << "] "
                  << t.id << " \t'" << t.valor << "'";
        if (t.id != "$") {
            std::cout << " \tlinea " << rl.posiciones[i].linea
                      << ", col " << rl.posiciones[i].columna;
        }
        std::cout << "\n";
    }
}

static void imprimirGramatica(const Gramatica& g) {
    std::cout << "Simbolo inicial: " << g.simboloInicial << "\n\n";

    std::cout << "Terminales: ";
    bool primero = true;
    for (const auto& t : g.terminales) {
        if (!primero) std::cout << ", ";
        std::cout << t;
        primero = false;
    }
    std::cout << "\n\n";

    std::cout << "No terminales: ";
    primero = true;
    for (const auto& nt : g.noTerminales) {
        if (!primero) std::cout << ", ";
        std::cout << nt;
        primero = false;
    }
    std::cout << "\n\n";

    std::cout << "Producciones:\n";
    for (size_t i = 0; i < g.producciones.size(); i++) {
        const Produccion& p = g.producciones[i];
        std::cout << "  " << i << ": " << p.izquierda << " ->";
        for (const auto& s : p.derecha) std::cout << " " << s;
        std::cout << "\n";
    }
}

// ─── leer archivo ─────────────────────────────────────────────────────────────

static std::string leerArchivo(const std::string& ruta) {
    std::ifstream f(ruta);
    if (!f.is_open()) {
        throw std::runtime_error("No se pudo abrir el archivo: " + ruta);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Uso: " << argv[0] << " <archivo.yapar> <archivo_entrada>\n";
        std::cerr << "Ejemplo: " << argv[0] << " parser.yapar entrada.txt\n";
        return 1;
    }

    const std::string rutaYapar   = argv[1];
    const std::string rutaEntrada = argv[2];

    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║  Pipeline: Lexer -> YAPar -> FIRST/FOLLOW -> LL(1) ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    std::cout << "  YAPar:   " << rutaYapar   << "\n";
    std::cout << "  Entrada: " << rutaEntrada << "\n";

    // ── Paso 1: Leer el archivo de entrada ───────────────────────────────────
    std::string textoEntrada;
    try {
        textoEntrada = leerArchivo(rutaEntrada);
    } catch (const std::exception& e) {
        std::cerr << "Error leyendo entrada: " << e.what() << "\n";
        return 1;
    }

    // ── Paso 2: Ejecutar el lexer generado ───────────────────────────────────
    imprimirSeparador("Paso 1-6: Análisis léxico");

    ResultadoLexico salidaLexer = analizar_tokens(textoEntrada);
    imprimirTokens(salidaLexer, "Tokens del lexer (antes de filtrar)");

    // ── Paso 3: Leer el archivo YAPar ────────────────────────────────────────
    imprimirSeparador("Paso 7-10: Lectura de YAPar");

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
        std::cout << t;
        primero = false;
    }
    std::cout << "\n";

    std::cout << "Tokens ignorados (IGNORE):  ";
    primero = true;
    for (const auto& t : spec.tokensIgnorados) {
        if (!primero) std::cout << ", ";
        std::cout << t;
        primero = false;
    }
    if (spec.tokensIgnorados.empty()) std::cout << "(ninguno)";
    std::cout << "\n";

    // ── Paso 4: Validar consistencia de IGNORE ───────────────────────────────
    try {
        validarTokensIgnorados(spec);
    } catch (const std::exception& e) {
        std::cerr << "Error de validación IGNORE: " << e.what() << "\n";
        return 1;
    }

    // ── Paso 5: Filtrar tokens ignorados ─────────────────────────────────────
    imprimirSeparador("Paso 11: Filtrado de tokens ignorados");

    ResultadoLexico filtrada = filtrarTokensIgnorados(salidaLexer, spec.tokensIgnorados);
    imprimirTokens(filtrada, "Tokens para el parser (después de filtrar)");

    // ── Paso 6: Validar tokens del lexer contra YAPar ────────────────────────
    imprimirSeparador("Paso 12: Validación YALex vs YAPar");

    std::cout << "Verificando que todos los tokens del lexer estén declarados en YAPar...\n";
    validarTokensDeEntrada(filtrada.tokens, filtrada.posiciones, spec);
    std::cout << "  (errores de validación aparecen arriba si los hay)\n";

    advertirTokensDeclaradosNoUsados(filtrada.tokens, spec);
    std::cout << "  (advertencias de tokens no usados aparecen arriba si las hay)\n";

    // ── Paso 7: Construir gramática ───────────────────────────────────────────
    imprimirSeparador("Paso 13: Gramática");

    Gramatica gramatica;
    try {
        gramatica = construirGramatica(spec);
    } catch (const std::exception& e) {
        std::cerr << "Error construyendo gramática: " << e.what() << "\n";
        return 1;
    }

    imprimirGramatica(gramatica);

    // ── Paso 8: Calcular FIRST ────────────────────────────────────────────────
    imprimirSeparador("Paso 14: FIRST");

    MapaFirst first = calcularFirst(gramatica);
    imprimirFirst(first, gramatica);

    // ── Paso 9: Calcular FOLLOW ───────────────────────────────────────────────
    imprimirSeparador("Paso 15: FOLLOW");

    MapaFollow follow = calcularFollow(gramatica, first);
    imprimirFollow(follow);

    // ── Paso 10: Construir tabla LL(1) ────────────────────────────────────────
    imprimirSeparador("Paso 16: Tabla LL(1)");

    ResultadoTablaLL1 resultado = construirTablaLL1(gramatica, first, follow);
    imprimirTablaLL1(resultado, gramatica);

    if (!resultado.esLL1()) {
        std::cout << "\n";
        imprimirConflictos(resultado.conflictos, gramatica);
    }

    // ── Resumen final ─────────────────────────────────────────────────────────
    imprimirSeparador("Resumen");

    std::cout << "  Tokens en entrada:       " << salidaLexer.tokens.size() - 1 << " (sin $)\n";
    std::cout << "  Tokens tras filtrado:    " << filtrada.tokens.size() - 1    << " (sin $)\n";
    std::cout << "  Producciones:            " << gramatica.producciones.size()  << "\n";
    std::cout << "  Celdas en tabla LL(1):   ";
    {
        int celdas = 0;
        for (auto& fila : resultado.tabla)
            for (auto& col : fila.second)
                if (col.second != -1) celdas++;
        std::cout << celdas << "\n";
    }
    std::cout << "  Conflictos LL(1):        " << resultado.conflictos.size() << "\n";
    std::cout << "  Gramática LL(1):         " << (resultado.esLL1() ? "SI" : "NO") << "\n";

    return resultado.esLL1() ? 0 : 2;
}