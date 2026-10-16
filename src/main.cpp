#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ast/printer.h"
#include "compiler/compiler.h"
#include "diagnostics/diagnostic.h"
#include "ir/tac_printer.h"
#include "semantic/printer.h"

namespace fs = std::filesystem;

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("No se pudo abrir " + path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static void printDiagnostics(const compiscript::compiler::CompilationResult& result) {
    using namespace compiscript;
    if (result.diagnostics.empty()) return;
    std::cout << "Diagnosticos:\n";
    for (const auto& d : result.diagnostics) {
        const char* sev = d.severity == diagnostics::Severity::Error ? "error" : "warning";
        std::cout << "  [" << d.code << "] " << sev << " " << d.line << ":" << d.column << ": "
                   << d.message << "\n";
    }
}

// Uso:
//   compiscript <archivo.cps>         salida completa: diagnosticos, AST, tabla
//                                     de simbolos y codigo intermedio
//   compiscript --tac <archivo.cps>   solo el TAC (o solo los diagnosticos si
//                                     hubo errores); es lo que usan las pruebas
int main(int argc, char* argv[]) {
    bool onlyTac = argc == 3 && std::strcmp(argv[1], "--tac") == 0;
    if (argc != 2 && !onlyTac) {
        std::cerr << "Uso: " << argv[0] << " [--tac] <archivo.cps>\n";
        return 1;
    }
    const char* path = argv[argc - 1];

    std::string source;
    try {
        source = readFile(path);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    using namespace compiscript;
    compiler::CompilationResult result = compiler::Compiler::compile(source);

    if (onlyTac) {
        if (result.has_tac) {
            std::cout << ir::printProgram(result.tac);
        } else {
            printDiagnostics(result);
        }
        return result.success() ? 0 : 1;
    }

    printDiagnostics(result);

    if (result.ast == nullptr) {
        std::cout << "No se genero AST (error sintactico).\n";
        return 1;
    }

    std::cout << "\nAST:\n" << ast::printTree(result.ast.get());

    std::cout << "\nTabla de simbolos:\n"
               << semantic::printScopeTree(result.symbol_table.global());

    fs::create_directories("output");
    std::ofstream dotFile("output/ast.dot");
    dotFile << ast::toDot(result.ast.get());
    std::cout << "\n[AST-DOT] Exportado a: output/ast.dot\n";

    // El TAC va despues del pie [AST-DOT] para no alterar las secciones que
    // la IDE ya separa (ver extraer_bloque en ide/ide_app.py).
    if (result.has_tac) {
        std::string tac = ir::printProgram(result.tac);
        std::cout << "\nCodigo intermedio:\n" << tac;
        std::ofstream tacFile("output/programa.tac");
        tacFile << tac;
        std::cout << "\n[TAC] Exportado a: output/programa.tac\n";
    }

    return result.success() ? 0 : 1;
}
