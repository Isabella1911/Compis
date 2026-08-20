/*
 * main_avance4.cpp
 *
 * Driver de prueba para el Avance 4:
 * Validación de tokens YALex-YAPar y preparación del análisis LL(1).
 *
 * Cubre:
 *   - validarTokensDeEntrada
 *   - validarTokensIgnorados
 *   - advertirTokensDeclaradosNoUsados
 *   - terminalesParaParsing
 *   - calcularFirst / calcularFollow
 *   - construirTablaLL1 (incluyendo detección de conflictos)
 *   - Prueba mínima del markdown (expr -> ID PLUS NUMBER)
 *   - Prueba con epsilon del markdown (expr_prime -> epsilon)
 *
 * Compilar:
 *   g++ -std=c++17 -o main_avance4 \
 *       main_avance4.cpp YaparParser.cpp Grammar.cpp FirstFollow.cpp LL1Table.cpp
 * Uso:
 *   ./main_avance4
 */

#include "YaparParser.h"
#include "FirstFollow.h"
#include "LL1Table.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <functional>

// ─── helpers ──────────────────────────────────────────────────────────────────

static int tests_ok   = 0;
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

// Redirige stderr a una cadena para verificar mensajes de advertencia/error
static std::string capturarStderr(std::function<void()> fn) {
    std::ostringstream buf;
    std::streambuf* original = std::cerr.rdbuf(buf.rdbuf());
    fn();
    std::cerr.rdbuf(original);
    return buf.str();
}

// ─── tests ────────────────────────────────────────────────────────────────────

void test_validar_tokens_de_entrada() {
    std::cout << "\n=== Test 1: validarTokensDeEntrada ===\n";

    YaparSpec spec;
    spec.tokensDeclarados = {"ID", "PLUS", "NUMBER"};

    std::vector<Token> tokens = {
        {"ID",     "x"},
        {"PLUS",   "+"},
        {"NUMBER", "5"},
        {"$",      "$"}
    };
    std::vector<TokenPosicion> posiciones = {{1,1},{1,3},{1,5},{1,6}};

    std::string salida = capturarStderr([&](){
        validarTokensDeEntrada(tokens, posiciones, spec);
    });

    if (salida.empty()) {
        ok("no hay errores con tokens válidos");
    } else {
        fail("tokens válidos", "se reportó error inesperado: " + salida);
    }

    // Introducir token desconocido
    std::vector<Token> tokensConError = {
        {"ID", "x"}, {"UNKNOWN", "??"}, {"$", "$"}
    };
    std::vector<TokenPosicion> posConError = {{1,1},{1,3},{1,5}};

    std::string salidaError = capturarStderr([&](){
        validarTokensDeEntrada(tokensConError, posConError, spec);
    });

    if (salidaError.find("UNKNOWN") != std::string::npos) {
        ok("error reportado para token no declarado (UNKNOWN)");
    } else {
        fail("token no declarado", "no se reportó el token UNKNOWN");
    }

    // $ nunca debe reportar error
    std::vector<Token> soloDolar = {{"$","$"}};
    std::vector<TokenPosicion> posDolar = {{1,1}};
    std::string salidaDolar = capturarStderr([&](){
        validarTokensDeEntrada(soloDolar, posDolar, spec);
    });
    if (salidaDolar.empty()) {
        ok("token $ no genera error de validación");
    } else {
        fail("token $", "se reportó error para $");
    }
}

void test_validar_tokens_ignorados() {
    std::cout << "\n=== Test 2: validarTokensIgnorados ===\n";

    YaparSpec spec;
    spec.tokensDeclarados = {"ID", "WS", "PLUS"};
    spec.tokensIgnorados  = {"WS"};

    try {
        validarTokensIgnorados(spec);
        ok("validación pasa con IGNORE correcto");
    } catch (...) {
        fail("IGNORE correcto", "lanzó excepción inesperada");
    }

    // Token ignorado no declarado
    YaparSpec specMal;
    specMal.tokensDeclarados = {"ID"};
    specMal.tokensIgnorados  = {"UNDECLARED"};

    try {
        validarTokensIgnorados(specMal);
        fail("token ignorado no declarado", "debería haber lanzado excepción");
    } catch (const std::runtime_error& e) {
        if (std::string(e.what()).find("UNDECLARED") != std::string::npos) {
            ok("error claro para token ignorado no declarado");
        } else {
            ok("excepción lanzada: " + std::string(e.what()));
        }
    }

    // Ignorar $ debe lanzar excepción
    YaparSpec specDolar;
    specDolar.tokensDeclarados = {"ID"};
    specDolar.tokensIgnorados  = {"$"};

    try {
        validarTokensIgnorados(specDolar);
        fail("ignorar $", "debería haber lanzado excepción");
    } catch (const std::runtime_error& e) {
        ok("excepción correcta al intentar ignorar $");
    }
}

void test_advertir_tokens_no_usados() {
    std::cout << "\n=== Test 3: advertirTokensDeclaradosNoUsados ===\n";

    YaparSpec spec;
    spec.tokensDeclarados = {"ID", "PLUS", "NUMBER", "MINUS"};
    spec.tokensIgnorados  = {};

    // MINUS declarado pero no producido
    std::vector<Token> tokens = {
        {"ID", "x"}, {"PLUS", "+"}, {"NUMBER", "5"}, {"$", "$"}
    };

    std::string salida = capturarStderr([&](){
        advertirTokensDeclaradosNoUsados(tokens, spec);
    });

    if (salida.find("MINUS") != std::string::npos) {
        ok("advertencia emitida para MINUS (declarado pero no usado)");
    } else {
        fail("advertencia MINUS", "no se emitió advertencia para MINUS");
    }

    if (salida.find("PLUS") == std::string::npos &&
        salida.find("ID") == std::string::npos &&
        salida.find("NUMBER") == std::string::npos) {
        ok("tokens usados no generan advertencia");
    } else {
        fail("tokens usados", "se generó advertencia para tokens que sí aparecen");
    }

    // Tokens ignorados no deben generar advertencia
    YaparSpec specIgn;
    specIgn.tokensDeclarados = {"ID", "WS"};
    specIgn.tokensIgnorados  = {"WS"};
    std::vector<Token> tokensIgn = {{"ID","x"}, {"$","$"}};

    std::string salidaIgn = capturarStderr([&](){
        advertirTokensDeclaradosNoUsados(tokensIgn, specIgn);
    });
    if (salidaIgn.find("WS") == std::string::npos) {
        ok("tokens ignorados no generan advertencia de no usados");
    } else {
        fail("ignorados no advierten", "WS (ignorado) generó advertencia");
    }
}

void test_terminales_para_parsing() {
    std::cout << "\n=== Test 4: terminalesParaParsing ===\n";

    YaparSpec spec;
    spec.tokensDeclarados = {"ID", "PLUS", "WS", "NUMBER"};
    spec.tokensIgnorados  = {"WS"};

    auto terminales = terminalesParaParsing(spec);

    if (terminales.count("ID") && terminales.count("PLUS") && terminales.count("NUMBER")) {
        ok("terminales significativos incluidos");
    } else {
        fail("terminales significativos", "falta ID, PLUS o NUMBER");
    }

    if (!terminales.count("WS")) {
        ok("WS (ignorado) excluido de terminalesParaParsing");
    } else {
        fail("excluir WS", "WS está en terminalesParaParsing pese a estar en IGNORE");
    }

    if (terminales.count("$")) {
        ok("$ siempre incluido en terminalesParaParsing");
    } else {
        fail("$ incluido", "$ no está en terminalesParaParsing");
    }
}

void test_first_basico() {
    std::cout << "\n=== Test 5: calcularFirst (gramática simple sin epsilon) ===\n";

    // expr -> ID PLUS NUMBER
    escribirArchivo("_t5.yapar",
        "%token ID PLUS NUMBER WS\n"
        "IGNORE WS\n"
        "%%\n"
        "expr:\n"
        "    ID PLUS NUMBER\n"
        ";\n"
    );

    try {
        YaparSpec spec = leerYapar("_t5.yapar");
        Gramatica g    = construirGramatica(spec);
        MapaFirst first = calcularFirst(g);

        // FIRST(expr) = {ID}
        auto& firstExpr = first["expr"];
        if (firstExpr.count("ID") && firstExpr.size() == 1) {
            ok("FIRST(expr) = { ID }");
        } else {
            fail("FIRST(expr)", "resultado incorrecto");
        }

        // FIRST de terminales son ellos mismos
        if (first["ID"].count("ID") && first["PLUS"].count("PLUS")) {
            ok("FIRST(terminal) = { terminal }");
        } else {
            fail("FIRST terminales", "FIRST de terminal no contiene a sí mismo");
        }

    } catch (const std::exception& e) {
        fail("FIRST básico", e.what());
    }
    std::remove("_t5.yapar");
}

void test_first_con_epsilon() {
    std::cout << "\n=== Test 6: calcularFirst (gramática con epsilon) ===\n";

    // expr       -> ID expr_prime
    // expr_prime -> PLUS ID expr_prime | epsilon
    escribirArchivo("_t6.yapar",
        "%token ID PLUS WS\n"
        "IGNORE WS\n"
        "%%\n"
        "expr:\n"
        "    ID expr_prime\n"
        ";\n"
        "expr_prime:\n"
        "    PLUS ID expr_prime\n"
        "  | epsilon\n"
        ";\n"
    );

    try {
        YaparSpec spec  = leerYapar("_t6.yapar");
        Gramatica g     = construirGramatica(spec);
        MapaFirst first = calcularFirst(g);

        // FIRST(expr) = {ID}
        if (first["expr"].count("ID") && first["expr"].size() == 1) {
            ok("FIRST(expr) = { ID }");
        } else {
            fail("FIRST(expr)", "incorrecto con epsilon");
        }

        // FIRST(expr_prime) = {PLUS, epsilon}
        auto& fprime = first["expr_prime"];
        if (fprime.count("PLUS") && fprime.count("epsilon") && fprime.size() == 2) {
            ok("FIRST(expr_prime) = { PLUS, epsilon }");
        } else {
            fail("FIRST(expr_prime)", "incorrecto: " + [&](){
                std::string s;
                for (auto& x : fprime) s += x + " ";
                return s;
            }());
        }

    } catch (const std::exception& e) {
        fail("FIRST con epsilon", e.what());
    }
    std::remove("_t6.yapar");
}

void test_follow_basico() {
    std::cout << "\n=== Test 7: calcularFollow (gramática simple) ===\n";

    escribirArchivo("_t7.yapar",
        "%token ID PLUS NUMBER WS\n"
        "IGNORE WS\n"
        "%%\n"
        "expr:\n"
        "    ID PLUS NUMBER\n"
        ";\n"
    );

    try {
        YaparSpec spec   = leerYapar("_t7.yapar");
        Gramatica g      = construirGramatica(spec);
        MapaFirst first  = calcularFirst(g);
        MapaFollow follow = calcularFollow(g, first);

        // FOLLOW(expr) = {$} (es el símbolo inicial)
        if (follow["expr"].count("$") && follow["expr"].size() == 1) {
            ok("FOLLOW(expr) = { $ }");
        } else {
            fail("FOLLOW(expr)", "incorrecto para símbolo inicial");
        }

    } catch (const std::exception& e) {
        fail("FOLLOW básico", e.what());
    }
    std::remove("_t7.yapar");
}

void test_follow_con_epsilon() {
    std::cout << "\n=== Test 8: calcularFollow (gramática con epsilon) ===\n";

    escribirArchivo("_t8.yapar",
        "%token ID PLUS WS\n"
        "IGNORE WS\n"
        "%%\n"
        "expr:\n"
        "    ID expr_prime\n"
        ";\n"
        "expr_prime:\n"
        "    PLUS ID expr_prime\n"
        "  | epsilon\n"
        ";\n"
    );

    try {
        YaparSpec spec   = leerYapar("_t8.yapar");
        Gramatica g      = construirGramatica(spec);
        MapaFirst first  = calcularFirst(g);
        MapaFollow follow = calcularFollow(g, first);

        // FOLLOW(expr) = {$}
        if (follow["expr"].count("$")) {
            ok("FOLLOW(expr) = { $ }");
        } else {
            fail("FOLLOW(expr)", "no contiene $");
        }

        // FOLLOW(expr_prime) = {$}
        // expr_prime aparece al final de expr y de sí misma
        if (follow["expr_prime"].count("$")) {
            ok("FOLLOW(expr_prime) = { $ }");
        } else {
            fail("FOLLOW(expr_prime)", "no contiene $");
        }

    } catch (const std::exception& e) {
        fail("FOLLOW con epsilon", e.what());
    }
    std::remove("_t8.yapar");
}

void test_tabla_ll1_minima() {
    std::cout << "\n=== Test 9: Prueba mínima del markdown (expr -> ID PLUS NUMBER) ===\n";

    escribirArchivo("_t9.yapar",
        "%token ID PLUS NUMBER WS\n"
        "IGNORE WS\n"
        "%%\n"
        "expr:\n"
        "    ID PLUS NUMBER\n"
        ";\n"
    );

    try {
        YaparSpec spec      = leerYapar("_t9.yapar");
        Gramatica g         = construirGramatica(spec);
        MapaFirst first     = calcularFirst(g);
        MapaFollow follow   = calcularFollow(g, first);
        auto resultado      = construirTablaLL1(g, first, follow);

        std::cout << "\n  --- Salida esperada del markdown ---\n";
        imprimirFirst(first, g);
        imprimirFollow(follow);
        imprimirTablaLL1(resultado, g);

        // M[expr, ID] debe apuntar a la producción 0
        int celda = resultado.tabla.at("expr").at("ID");
        if (celda == 0) {
            ok("M[expr, ID] = expr -> ID PLUS NUMBER (produccion 0)");
        } else {
            fail("M[expr, ID]", "valor incorrecto: " + std::to_string(celda));
        }

        if (resultado.esLL1()) {
            ok("gramática es LL(1) (sin conflictos)");
        } else {
            fail("LL(1)", "conflictos inesperados");
        }

    } catch (const std::exception& e) {
        fail("tabla LL(1) mínima", e.what());
    }
    std::remove("_t9.yapar");
}

void test_tabla_ll1_con_epsilon() {
    std::cout << "\n=== Test 10: Prueba con epsilon del markdown ===\n";

    escribirArchivo("_t10.yapar",
        "%token ID PLUS WS\n"
        "IGNORE WS\n"
        "%%\n"
        "expr:\n"
        "    ID expr_prime\n"
        ";\n"
        "expr_prime:\n"
        "    PLUS ID expr_prime\n"
        "  | epsilon\n"
        ";\n"
    );

    try {
        YaparSpec spec      = leerYapar("_t10.yapar");
        Gramatica g         = construirGramatica(spec);
        MapaFirst first     = calcularFirst(g);
        MapaFollow follow   = calcularFollow(g, first);
        auto resultado      = construirTablaLL1(g, first, follow);

        std::cout << "\n  --- Salida esperada del markdown ---\n";
        imprimirFirst(first, g);
        imprimirFollow(follow);
        imprimirTablaLL1(resultado, g);

        // M[expr, ID]
        int celdaExprID = resultado.tabla.at("expr").at("ID");
        if (celdaExprID >= 0) {
            ok("M[expr, ID] tiene entrada válida");
        } else {
            fail("M[expr, ID]", "celda vacía");
        }

        // M[expr_prime, PLUS]
        int celdaPrimePlus = resultado.tabla.at("expr_prime").at("PLUS");
        if (celdaPrimePlus >= 0) {
            ok("M[expr_prime, PLUS] tiene entrada válida");
        } else {
            fail("M[expr_prime, PLUS]", "celda vacía");
        }

        // M[expr_prime, $] debe usar la producción epsilon
        int celdaPrimeDolar = resultado.tabla.at("expr_prime").at("$");
        if (celdaPrimeDolar >= 0) {
            const Produccion& p = g.producciones[static_cast<size_t>(celdaPrimeDolar)];
            if (p.derecha.size() == 1 && p.derecha[0] == "epsilon") {
                ok("M[expr_prime, $] = expr_prime -> epsilon");
            } else {
                fail("M[expr_prime, $]", "no apunta a producción epsilon");
            }
        } else {
            fail("M[expr_prime, $]", "celda vacía, se esperaba epsilon");
        }

        if (resultado.esLL1()) {
            ok("gramática con epsilon es LL(1)");
        } else {
            fail("LL(1) con epsilon", "conflictos inesperados");
            imprimirConflictos(resultado.conflictos, g);
        }

    } catch (const std::exception& e) {
        fail("tabla LL(1) con epsilon", e.what());
    }
    std::remove("_t10.yapar");
}

void test_deteccion_conflicto_ll1() {
    std::cout << "\n=== Test 11: Detección de conflicto LL(1) (gramática ambigua) ===\n";

    // expr -> ID | ID PLUS ID  → conflicto en M[expr, ID]
    escribirArchivo("_t11.yapar",
        "%token ID PLUS\n"
        "%%\n"
        "expr:\n"
        "    ID\n"
        "  | ID PLUS ID\n"
        ";\n"
    );

    try {
        YaparSpec spec    = leerYapar("_t11.yapar");
        Gramatica g       = construirGramatica(spec);
        MapaFirst first   = calcularFirst(g);
        MapaFollow follow = calcularFollow(g, first);
        auto resultado    = construirTablaLL1(g, first, follow);

        if (!resultado.esLL1()) {
            ok("conflicto LL(1) detectado en gramática ambigua");

            bool tieneConflictoEnID = false;
            for (const auto& c : resultado.conflictos) {
                if (c.noTerminal == "expr" && c.terminal == "ID") {
                    tieneConflictoEnID = true;
                    break;
                }
            }
            if (tieneConflictoEnID) {
                ok("conflicto específico en M[expr, ID] identificado");
            } else {
                fail("conflicto M[expr,ID]", "conflicto no reportado en la celda correcta");
            }

            imprimirConflictos(resultado.conflictos, g);
        } else {
            fail("conflicto LL(1)", "debería haber detectado conflicto");
        }

    } catch (const std::exception& e) {
        fail("detección conflicto", e.what());
    }
    std::remove("_t11.yapar");
}

void test_flujo_completo_avance4() {
    std::cout << "\n=== Test 12: Flujo completo del avance 4 ===\n";

    escribirArchivo("_t12.yapar",
        "%token ID PLUS NUMBER WS\n"
        "IGNORE WS\n"
        "%%\n"
        "expr:\n"
        "    ID PLUS NUMBER\n"
        ";\n"
    );

    try {
        // 1-2. Leer YAPar
        YaparSpec spec = leerYapar("_t12.yapar");

        // 3. Simular salida del lexer para "x + 5"
        ResultadoLexico salidaLexer;
        salidaLexer.tokens = {
            {"ID","x"}, {"WS"," "}, {"PLUS","+"}, {"WS"," "}, {"NUMBER","5"}, {"$","$"}
        };
        salidaLexer.posiciones = {{1,1},{1,2},{1,3},{1,4},{1,5},{1,6}};

        // 4. Filtrar ignorados
        ResultadoLexico filtrada = filtrarTokensIgnorados(salidaLexer, spec.tokensIgnorados);

        // 5. Validar tokens contra YAPar
        std::string errores = capturarStderr([&](){
            validarTokensDeEntrada(filtrada.tokens, filtrada.posiciones, spec);
        });
        if (errores.empty()) {
            ok("tokens de entrada válidos contra YAPar");
        } else {
            fail("validación tokens entrada", errores);
        }

        // 6. Validar IGNORE
        validarTokensIgnorados(spec);
        ok("IGNORE válido");

        // 7. Advertir no usados
        std::string advs = capturarStderr([&](){
            advertirTokensDeclaradosNoUsados(filtrada.tokens, spec);
        });
        ok("advertencias de tokens no usados emitidas sin error fatal");

        // 8. Terminales para parsing (excluye WS)
        auto terminales = terminalesParaParsing(spec);
        if (!terminales.count("WS") && terminales.count("$")) {
            ok("terminalesParaParsing correcto");
        } else {
            fail("terminalesParaParsing", "WS incluido o $ excluido");
        }

        // 9. Construir gramática
        Gramatica g = construirGramatica(spec);
        ok("gramática construida y validada");

        // 10. FIRST / FOLLOW
        MapaFirst first   = calcularFirst(g);
        MapaFollow follow = calcularFollow(g, first);
        ok("FIRST y FOLLOW calculados");

        // 11. Tabla LL(1)
        auto resultado = construirTablaLL1(g, first, follow);
        if (resultado.esLL1()) {
            ok("tabla LL(1) construida sin conflictos");
        } else {
            fail("tabla LL(1)", "conflictos inesperados");
        }

        std::cout << "\n  --- Resumen del flujo completo ---\n";
        std::cout << "  Tokens filtrados: ";
        for (auto& t : filtrada.tokens) std::cout << t.id << " ";
        std::cout << "\n";
        imprimirFirst(first, g);
        imprimirFollow(follow);
        imprimirTablaLL1(resultado, g);

    } catch (const std::exception& e) {
        fail("flujo completo avance 4", e.what());
    }
    std::remove("_t12.yapar");
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "\n╔══════════════════════════════════════════════════╗\n";
    std::cout << "║  Avance 4: Validación tokens y preparación LL(1) ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n";

    test_validar_tokens_de_entrada();
    test_validar_tokens_ignorados();
    test_advertir_tokens_no_usados();
    test_terminales_para_parsing();
    test_first_basico();
    test_first_con_epsilon();
    test_follow_basico();
    test_follow_con_epsilon();
    test_tabla_ll1_minima();
    test_tabla_ll1_con_epsilon();
    test_deteccion_conflicto_ll1();
    test_flujo_completo_avance4();

    std::cout << "\n══════════════════════════════════════════════════\n";
    std::cout << "Resultado: " << tests_ok << " OK, " << tests_fail << " FAIL\n";

    return tests_fail == 0 ? 0 : 1;
}
