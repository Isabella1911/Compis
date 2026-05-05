/*
 * main_avance2.cpp
 *
 * Driver de prueba para el Avance 2:
 * Soporte de IGNORE y filtrado de tokens para parsing.
 *
 * Verifica:
 *   - Lectura de sección %token desde archivo YAPar
 *   - Lectura de IGNORE
 *   - Eliminación de comentarios en YAPar
 *   - Filtrado de tokens ignorados por id
 *   - Conservación del token $
 *   - Alineación de vectores tokens/posiciones
 *   - Mensajes de error claros en casos inválidos
 *
 * Compilar:
 *   g++ -std=c++17 -o main_avance2 main_avance2.cpp YaparParser.cpp Grammar.cpp
 * Uso:
 *   ./main_avance2
 */

#include "YaparParser.h"
#include <iostream>
#include <fstream>

// ─── helpers ──────────────────────────────────────────────────────────────────

static int tests_ok  = 0;
static int tests_fail = 0;

static void ok(const std::string& nombre) {
    std::cout << "  OK  " << nombre << "\n";
    tests_ok++;
}

static void fail(const std::string& nombre, const std::string& detalle) {
    std::cerr << "  FAIL " << nombre << ": " << detalle << "\n";
    tests_fail++;
}

static void escribirArchivo(const std::string& ruta, const std::string& contenido) {
    std::ofstream f(ruta);
    if (!f.is_open()) throw std::runtime_error("No se pudo crear " + ruta);
    f << contenido;
}

// ─── tests ────────────────────────────────────────────────────────────────────

void test_lectura_tokens_basica() {
    std::cout << "\n=== Test 1: Lectura básica de %token e IGNORE ===\n";

    escribirArchivo("_t1.yapar",
        "%token ID NUMBER PLUS WS COMMENT\n"
        "IGNORE WS COMMENT\n"
        "%%\n"
        "expr:\n"
        "    ID PLUS NUMBER\n"
        ";\n"
    );

    try {
        YaparSpec spec = leerYapar("_t1.yapar");

        if (spec.tokensDeclarados.count("ID") &&
            spec.tokensDeclarados.count("NUMBER") &&
            spec.tokensDeclarados.count("PLUS") &&
            spec.tokensDeclarados.count("WS") &&
            spec.tokensDeclarados.count("COMMENT")) {
            ok("tokens declarados correctamente (5 tokens)");
        } else {
            fail("tokens declarados", "faltan tokens en tokensDeclarados");
        }

        if (spec.tokensIgnorados.count("WS") && spec.tokensIgnorados.count("COMMENT")) {
            ok("tokens ignorados registrados (WS, COMMENT)");
        } else {
            fail("tokens ignorados", "WS o COMMENT no están en tokensIgnorados");
        }

        if (!spec.tokensIgnorados.count("ID") && !spec.tokensIgnorados.count("PLUS")) {
            ok("tokens significativos no están en ignorados");
        } else {
            fail("tokens significativos", "ID o PLUS aparecen en ignorados por error");
        }

        std::cout << "  Símbolo inicial: " << spec.simboloInicial << "\n";
        std::cout << "  Producciones: " << spec.producciones.size() << "\n";

    } catch (const std::exception& e) {
        fail("lectura básica", e.what());
    }
}

void test_multiples_tokens_en_linea() {
    std::cout << "\n=== Test 2: Múltiples tokens en una línea ===\n";

    YaparSpec spec;
    procesarLineaToken("%token ID NUMBER PLUS MINUS TIMES DIV", spec);

    if (spec.tokensDeclarados.size() == 6) {
        ok("6 tokens declarados en una sola línea %token");
    } else {
        fail("múltiples tokens", "se esperaban 6, se obtuvo " +
            std::to_string(spec.tokensDeclarados.size()));
    }

    procesarLineaIgnore("IGNORE PLUS MINUS", spec);

    if (spec.tokensIgnorados.size() == 2) {
        ok("2 tokens ignorados en una sola línea IGNORE");
    } else {
        fail("múltiples ignorados", "se esperaban 2, se obtuvo " +
            std::to_string(spec.tokensIgnorados.size()));
    }
}

void test_eliminacion_comentarios() {
    std::cout << "\n=== Test 3: Eliminación de comentarios /* ... */ ===\n";

    std::string con_comentarios =
        "/* esto es un comentario */\n"
        "%token ID /* otro comentario */ PLUS\n"
        "/* comentario\n"
        "   multilinea */\n"
        "IGNORE\n"
        "%%\n"
        "expr: ID PLUS ID ;\n";

    try {
        std::string limpio = eliminarComentariosYapar(con_comentarios);

        if (limpio.find("esto") == std::string::npos &&
            limpio.find("otro comentario") == std::string::npos &&
            limpio.find("multilinea") == std::string::npos) {
            ok("comentarios eliminados correctamente");
        } else {
            fail("eliminación comentarios", "quedaron restos de comentarios en el texto");
        }

        if (limpio.find("%token") != std::string::npos &&
            limpio.find("PLUS") != std::string::npos) {
            ok("contenido útil conservado tras eliminar comentarios");
        } else {
            fail("contenido útil", "se perdió contenido que no era comentario");
        }

    } catch (const std::exception& e) {
        fail("eliminación comentarios", e.what());
    }
}

void test_comentario_sin_cierre() {
    std::cout << "\n=== Test 4: Comentario sin cierre (debe fallar) ===\n";

    std::string mal = "/* comentario sin cerrar\n%token ID\n%%\nexpr: ID;\n";

    try {
        eliminarComentariosYapar(mal);
        fail("comentario sin cierre", "debería haber lanzado excepción");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        if (msg.find("sin cierre") != std::string::npos ||
            msg.find("*/") != std::string::npos) {
            ok("error claro: " + msg);
        } else {
            ok("excepción lanzada (mensaje: " + msg + ")");
        }
    }
}

void test_filtrado_tokens() {
    std::cout << "\n=== Test 5: Filtrado de tokens ignorados ===\n";

    ResultadoLexico entrada;
    entrada.tokens = {
        {"ID",      "x"},
        {"WS",      " "},
        {"PLUS",    "+"},
        {"WS",      " "},
        {"NUMBER",  "5"},
        {"COMMENT", "// ok"},
        {"$",       "$"}
    };
    entrada.posiciones = {
        {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7}
    };

    std::set<std::string> ignorados = {"WS", "COMMENT"};
    ResultadoLexico filtrada = filtrarTokensIgnorados(entrada, ignorados);

    // Debe quedar: ID, PLUS, NUMBER, $
    if (filtrada.tokens.size() == 4) {
        ok("tamaño correcto después del filtrado (4 tokens)");
    } else {
        fail("tamaño filtrado", "se esperaban 4, se obtuvo " +
            std::to_string(filtrada.tokens.size()));
    }

    if (filtrada.tokens[0].id == "ID" &&
        filtrada.tokens[1].id == "PLUS" &&
        filtrada.tokens[2].id == "NUMBER") {
        ok("orden y ids correctos después del filtrado");
    } else {
        fail("ids filtrados", "el orden o ids son incorrectos");
    }

    if (filtrada.tokens.back().id == "$") {
        ok("token $ conservado al final");
    } else {
        fail("token $", "el token $ no está al final");
    }

    if (filtrada.tokens.size() == filtrada.posiciones.size()) {
        ok("vectores tokens y posiciones alineados");
    } else {
        fail("alineación vectores", "tokens y posiciones tienen tamaños distintos");
    }

    // Verificar que las posiciones son las correctas (línea 1, col 1 para ID)
    if (filtrada.posiciones[0].linea == 1 && filtrada.posiciones[0].columna == 1) {
        ok("posición de ID conservada correctamente (línea 1, col 1)");
    } else {
        fail("posición ID", "la posición no coincide con la original");
    }
}

void test_token_dolar_no_se_filtra() {
    std::cout << "\n=== Test 6: El token $ no se filtra aunque esté en ignorados ===\n";

    ResultadoLexico entrada;
    entrada.tokens = {{"ID", "x"}, {"$", "$"}};
    entrada.posiciones = {{1, 1}, {1, 2}};

    // Aunque se pase $ en el set de ignorados, no debe eliminarse
    std::set<std::string> ignorados = {"$", "ID"};
    ResultadoLexico filtrada = filtrarTokensIgnorados(entrada, ignorados);

    if (!filtrada.tokens.empty() && filtrada.tokens.back().id == "$") {
        ok("token $ siempre permanece al final aunque esté en ignorados");
    } else {
        fail("token $ protegido", "$ fue eliminado incorrectamente");
    }
}

void test_error_ignore_token_no_declarado() {
    std::cout << "\n=== Test 7: IGNORE con token no declarado (debe fallar) ===\n";

    escribirArchivo("_t7.yapar",
        "%token ID PLUS\n"
        "IGNORE WS\n"
        "%%\n"
        "expr: ID PLUS ID;\n"
    );

    try {
        leerYapar("_t7.yapar");
        fail("token no declarado en IGNORE", "debería haber lanzado excepción");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        if (msg.find("WS") != std::string::npos) {
            ok("error claro menciona el token problemático: " + msg);
        } else {
            ok("excepción lanzada: " + msg);
        }
    }
}

void test_error_ignore_antes_de_token() {
    std::cout << "\n=== Test 8: IGNORE antes de %token (debe fallar) ===\n";

    escribirArchivo("_t8.yapar",
        "IGNORE WS\n"
        "%token ID WS\n"
        "%%\n"
        "expr: ID;\n"
    );

    try {
        leerYapar("_t8.yapar");
        fail("IGNORE antes de %token", "debería haber lanzado excepción");
    } catch (const std::runtime_error& e) {
        ok("excepción lanzada: " + std::string(e.what()));
    }
}

void test_error_falta_separador() {
    std::cout << "\n=== Test 9: Archivo sin separador %% (debe fallar) ===\n";

    escribirArchivo("_t9.yapar",
        "%token ID PLUS\n"
        "expr: ID PLUS ID;\n"
    );

    try {
        leerYapar("_t9.yapar");
        fail("falta %%", "debería haber lanzado excepción");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        if (msg.find("%%") != std::string::npos || msg.find("separador") != std::string::npos) {
            ok("error claro por falta de %%: " + msg);
        } else {
            ok("excepción lanzada: " + msg);
        }
    }
}

void test_no_falso_positivo_nombre_token() {
    std::cout << "\n=== Test 10: Token llamado IGNOREDTOKEN no confunde al parser ===\n";

    escribirArchivo("_t10.yapar",
        "%token ID IGNOREDTOKEN PLUS\n"
        "%%\n"
        "expr: ID PLUS IGNOREDTOKEN;\n"
    );

    try {
        YaparSpec spec = leerYapar("_t10.yapar");

        if (spec.tokensDeclarados.count("IGNOREDTOKEN")) {
            ok("IGNOREDTOKEN declarado como token normal");
        } else {
            fail("IGNOREDTOKEN", "no se declaró correctamente");
        }

        if (spec.tokensIgnorados.empty()) {
            ok("ningún token en ignorados (IGNOREDTOKEN no es IGNORE)");
        } else {
            fail("falso positivo", "IGNOREDTOKEN fue confundido con la palabra clave IGNORE");
        }

    } catch (const std::exception& e) {
        fail("falso positivo IGNORE", e.what());
    }
}

void test_flujo_completo_con_lexer_simulado() {
    std::cout << "\n=== Test 11: Flujo completo con lexer simulado ===\n";

    escribirArchivo("_t11.yapar",
        "/* Parser de expresiones simples */\n"
        "%token ID NUMBER PLUS WS EOL\n"
        "IGNORE WS EOL\n"
        "%%\n"
        "expr:\n"
        "    expr PLUS term\n"
        "  | term\n"
        ";\n"
        "term:\n"
        "    ID\n"
        "  | NUMBER\n"
        ";\n"
    );

    try {
        YaparSpec spec = leerYapar("_t11.yapar");

        // Simular salida del lexer generado para "x + 5"
        ResultadoLexico salidaLexer;
        salidaLexer.tokens = {
            {"ID",     "x"},
            {"WS",     " "},
            {"PLUS",   "+"},
            {"WS",     " "},
            {"NUMBER", "5"},
            {"EOL",    "\n"},
            {"$",      "$"}
        };
        salidaLexer.posiciones = {
            {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {2, 1}
        };

        ResultadoLexico filtrada = filtrarTokensIgnorados(salidaLexer, spec.tokensIgnorados);

        std::cout << "  Tokens antes del filtrado: " << salidaLexer.tokens.size() << "\n";
        std::cout << "  Tokens después del filtrado: " << filtrada.tokens.size() << "\n";
        std::cout << "  Secuencia para el parser: ";
        for (const auto& t : filtrada.tokens) {
            std::cout << t.id;
            if (t.id != "$") std::cout << " ";
        }
        std::cout << "\n";

        // Debe quedar: ID PLUS NUMBER $
        bool correcto = (filtrada.tokens.size() == 4 &&
                         filtrada.tokens[0].id == "ID" &&
                         filtrada.tokens[1].id == "PLUS" &&
                         filtrada.tokens[2].id == "NUMBER" &&
                         filtrada.tokens[3].id == "$");

        if (correcto) {
            ok("flujo completo produce secuencia correcta para el parser");
        } else {
            fail("flujo completo", "la secuencia filtrada no es la esperada");
        }

        if (filtrada.posiciones[0].linea == 1 && filtrada.posiciones[0].columna == 1) {
            ok("posiciones correctas después del filtrado");
        } else {
            fail("posiciones flujo completo", "posición incorrecta para ID");
        }

    } catch (const std::exception& e) {
        fail("flujo completo", e.what());
    }
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║  Avance 2: IGNORE y filtrado de tokens      ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    test_lectura_tokens_basica();
    test_multiples_tokens_en_linea();
    test_eliminacion_comentarios();
    test_comentario_sin_cierre();
    test_filtrado_tokens();
    test_token_dolar_no_se_filtra();
    test_error_ignore_token_no_declarado();
    test_error_ignore_antes_de_token();
    test_error_falta_separador();
    test_no_falso_positivo_nombre_token();
    test_flujo_completo_con_lexer_simulado();

    std::cout << "\n══════════════════════════════════════════════\n";
    std::cout << "Resultado: " << tests_ok << " OK, " << tests_fail << " FAIL\n";

    // Limpiar archivos temporales
    std::remove("_t1.yapar");
    std::remove("_t7.yapar");
    std::remove("_t8.yapar");
    std::remove("_t9.yapar");
    std::remove("_t10.yapar");
    std::remove("_t11.yapar");

    return tests_fail == 0 ? 0 : 1;
}
