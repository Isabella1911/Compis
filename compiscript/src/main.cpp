#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ast/printer.h"
#include "compiler/compiler.h"
#include "diagnostics/diagnostic.h"
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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <archivo.cps>\n";
        return 1;
    }

    std::string source;
    try {
        source = readFile(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    using namespace compiscript;
    compiler::CompilationResult result = compiler::Compiler::compile(source);

    if (!result.diagnostics.empty()) {
        std::cout << "Diagnosticos:\n";
        for (const auto& d : result.diagnostics) {
            const char* sev = d.severity == diagnostics::Severity::Error ? "error" : "warning";
            std::cout << "  [" << d.code << "] " << sev << " " << d.line << ":" << d.column
                       << ": " << d.message << "\n";
        }
    }

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

    return result.success() ? 0 : 1;
}
