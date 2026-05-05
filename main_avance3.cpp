/*
 * Avance 3: Parser recibe tokens y construye gramática
 *
 * Este archivo demuestra la integración completa del pipeline:
 * 1. YaparParser lee archivo YAPar
 * 2. Filtra tokens ignorados
 * 3. Construye y valida gramática
 * 4. ParserDriver gestiona el stream de tokens
 */

#include "Grammar.h"
#include "ParserDriver.h"
#include "YaparParser.h"
#include <iostream>
#include <fstream>

void crearArchivoYaparEjemplo(const std::string& ruta) {
    std::ofstream f(ruta);
    if (!f.is_open()) {
        throw std::runtime_error("No se pudo crear archivo YAPar de prueba");
    }

    f << "/* Definición de parser */\n";
    f << "/* INICIA Sección de TOKENS */\n";
    f << "%token ID PLUS NUMBER WS COMMENT\n";
    f << "IGNORE WS COMMENT\n";
    f << "/* FINALIZA Sección de TOKENS */\n";
    f << "\n";
    f << "%%\n";
    f << "\n";
    f << "/* INICIA Sección de PRODUCCIONES */\n";
    f << "expr:\n";
    f << "    expr PLUS term\n";
    f << "  | term\n";
    f << ";\n";
    f << "\n";
    f << "term:\n";
    f << "    ID\n";
    f << "  | NUMBER\n";
    f << ";\n";
    f << "/* FINALIZA Sección de PRODUCCIONES */\n";

    f.close();
}

void prueba_parseo_yapar() {
    std::cout << "=== Test 1: Parseo de archivo YAPar ===\n";

    try {
        std::string rutaYapar = "parser_test.yapar";
        crearArchivoYaparEjemplo(rutaYapar);

        YaparSpec spec = leerYapar(rutaYapar);

        std::cout << "Tokens declarados: ";
        for (const auto& t : spec.tokensDeclarados) {
            std::cout << t << " ";
        }
        std::cout << "\n";

        std::cout << "Tokens ignorados: ";
        for (const auto& t : spec.tokensIgnorados) {
            std::cout << t << " ";
        }
        std::cout << "\n";

        std::cout << "Símbolo inicial: " << spec.simboloInicial << "\n";
        std::cout << "Producciones: " << spec.producciones.size() << "\n";

        for (const auto& prod : spec.producciones) {
            std::cout << "  " << prod.izquierda << " -> ";
            for (const auto& s : prod.derecha) {
                std::cout << s << " ";
            }
            std::cout << "\n";
        }

        std::cout << "✓ Test 1 pasó\n\n";
    } catch (const std::exception& e) {
        std::cerr << "✗ Test 1 falló: " << e.what() << "\n\n";
    }
}

void prueba_construccion_gramatica() {
    std::cout << "=== Test 2: Construcción de gramática ===\n";

    try {
        std::string rutaYapar = "parser_test.yapar";
        YaparSpec spec = leerYapar(rutaYapar);
        Gramatica g = construirGramatica(spec);

        std::cout << "Terminales: ";
        for (const auto& t : g.terminales) {
            std::cout << t << " ";
        }
        std::cout << "\n";

        std::cout << "No-terminales: ";
        for (const auto& nt : g.noTerminales) {
            std::cout << nt << " ";
        }
        std::cout << "\n";

        std::cout << "Símbolo inicial: " << g.simboloInicial << "\n";
        std::cout << "✓ Test 2 pasó\n\n";
    } catch (const std::exception& e) {
        std::cerr << "✗ Test 2 falló: " << e.what() << "\n\n";
    }
}

void prueba_filtrado_tokens() {
    std::cout << "=== Test 3: Filtrado de tokens ignorados ===\n";

    try {
        ResultadoLexico entrada;
        entrada.tokens = {
            {"ID", "x"},
            {"WS", " "},
            {"PLUS", "+"},
            {"WS", " "},
            {"NUMBER", "5"},
            {"COMMENT", "// comentario"},
            {"$", "$"}
        };
        entrada.posiciones = {
            {1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}
        };

        std::set<std::string> ignorados = {"WS", "COMMENT"};
        ResultadoLexico filtrada = filtrarTokensIgnorados(entrada, ignorados);

        std::cout << "Tokens antes del filtrado: " << entrada.tokens.size() << "\n";
        std::cout << "Tokens después del filtrado: " << filtrada.tokens.size() << "\n";

        std::cout << "Tokens filtrados:\n";
        for (size_t i = 0; i < filtrada.tokens.size(); i++) {
            std::cout << "  " << filtrada.tokens[i].id << " '" << filtrada.tokens[i].valor << "'\n";
        }

        if (filtrada.tokens.back().id == "$") {
            std::cout << "✓ Token $ preservado al final\n";
        }

        std::cout << "✓ Test 3 pasó\n\n";
    } catch (const std::exception& e) {
        std::cerr << "✗ Test 3 falló: " << e.what() << "\n\n";
    }
}

void prueba_parser_driver() {
    std::cout << "=== Test 4: ParserDriver ===\n";

    try {
        std::vector<Token> tokens = {
            {"ID", "x"},
            {"PLUS", "+"},
            {"NUMBER", "5"},
            {"$", "$"}
        };
        std::vector<TokenPosicion> posiciones = {
            {1, 0}, {1, 2}, {1, 4}, {1, 6}
        };

        ParserDriver driver(tokens, posiciones);

        std::cout << "Token actual: " << driver.verActual().id << "\n";

        while (!driver.fin()) {
            std::cout << "  Consumiendo: " << driver.verActual().id << " en línea "
                      << driver.obtenerPosicion().linea << ", columna "
                      << driver.obtenerPosicion().columna << "\n";
            driver.avanzar();
        }

        std::cout << "✓ Test 4 pasó\n\n";
    } catch (const std::exception& e) {
        std::cerr << "✗ Test 4 falló: " << e.what() << "\n\n";
    }
}

void prueba_validacion_gramatica_error() {
    std::cout << "=== Test 5: Validación de gramática (debe fallar) ===\n";

    try {
        Gramatica g;
        g.terminales = {"ID", "PLUS"};
        g.noTerminales = {"expr"};
        g.simboloInicial = "expr";
        g.producciones.push_back({"expr", {"ID", "UNDEFINED", "PLUS"}});

        validarGramatica(g);

        std::cout << "✗ Test 5 falló: debería haber lanzado excepción\n\n";
    } catch (const std::exception& e) {
        std::cout << "✓ Test 5 pasó: " << e.what() << "\n\n";
    }
}

int main() {
    std::cout << "\n╔════════════════════════════════════════╗\n";
    std::cout << "║  Avance 3: Tests de Integración       ║\n";
    std::cout << "╚════════════════════════════════════════╝\n\n";

    prueba_parseo_yapar();
    prueba_construccion_gramatica();
    prueba_filtrado_tokens();
    prueba_parser_driver();
    prueba_validacion_gramatica_error();

    std::cout << "════════════════════════════════════════\n";
    std::cout << "Todos los tests completados.\n";

    return 0;
}
