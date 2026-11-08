#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ast/printer.h"
#include "codegen/mips/mips_generator.h"
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

static void printDiagnostics(const std::vector<compiscript::diagnostics::Diagnostic>& diags,
                             std::ostream& out) {
    if (diags.empty()) return;
    out << "Diagnosticos:\n";
    for (const auto& d : diags) {
        const char* sev = d.severity == compiscript::diagnostics::Severity::Error ? "error"
                                                                                    : "warning";
        out << "  [" << d.code << "] " << sev << " " << d.line << ":" << d.column << ": "
            << d.message << "\n";
    }
}

static void printDiagnostics(const compiscript::compiler::CompilationResult& result) {
    printDiagnostics(result.diagnostics, std::cout);
}

static void usage(const char* argv0) {
    std::cerr << "Uso:\n"
              << "  " << argv0 << " <archivo.cps>\n"
              << "  " << argv0 << " --tac <archivo.cps>\n"
              << "  " << argv0 << " --mips <archivo.cps> [-o <salida.asm>]\n";
}

// Uso:
//   compiscript <archivo.cps>              salida completa
//   compiscript --tac <archivo.cps>        solo TAC (o diagnosticos)
//   compiscript --mips <archivo.cps> [-o]  solo ensamblador MIPS (o diagnosticos)
int main(int argc, char* argv[]) {
    bool onlyTac = false;
    bool onlyMips = false;
    std::string outAsm;
    std::string path;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tac") == 0) {
            onlyTac = true;
        } else if (std::strcmp(argv[i], "--mips") == 0) {
            onlyMips = true;
        } else if (std::strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "Error: -o requiere una ruta\n";
                usage(argv[0]);
                return 2;
            }
            outAsm = argv[++i];
        } else if (argv[i][0] == '-') {
            std::cerr << "Opcion desconocida: " << argv[i] << "\n";
            usage(argv[0]);
            return 2;
        } else if (path.empty()) {
            path = argv[i];
        } else {
            std::cerr << "Sobra el argumento: " << argv[i] << "\n";
            usage(argv[0]);
            return 2;
        }
    }

    if (path.empty() || (onlyTac && onlyMips)) {
        usage(argv[0]);
        return 2;
    }
    if (!outAsm.empty() && !onlyMips) {
        std::cerr << "Error: -o solo aplica con --mips\n";
        usage(argv[0]);
        return 2;
    }

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

    if (onlyMips) {
        // No generar .asm si el frontend/semantico fallaron.
        if (!result.has_tac || !result.success()) {
            printDiagnostics(result.diagnostics, std::cerr);
            if (!outAsm.empty() && fs::exists(outAsm)) {
                std::error_code ec;
                fs::remove(outAsm, ec);
            }
            return 1;
        }

        codegen::mips::MipsResult mips = codegen::mips::generateMips(result.tac);
        if (!mips.success) {
            printDiagnostics(mips.diagnostics, std::cerr);
            if (!outAsm.empty() && fs::exists(outAsm)) {
                std::error_code ec;
                fs::remove(outAsm, ec);
            }
            return 1;
        }

        if (outAsm.empty()) {
            std::cout << mips.assembly;
        } else {
            std::ofstream out(outAsm, std::ios::binary | std::ios::trunc);
            if (!out) {
                std::cerr << "Error: no se pudo escribir " << outAsm << "\n";
                return 1;
            }
            out << mips.assembly;
            if (!out) {
                std::cerr << "Error: fallo al escribir " << outAsm << "\n";
                fs::remove(outAsm);
                return 1;
            }
        }
        return 0;
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

    if (result.has_tac) {
        std::string tac = ir::printProgram(result.tac);
        std::cout << "\nCodigo intermedio:\n" << tac;
        std::ofstream tacFile("output/programa.tac");
        tacFile << tac;
        std::cout << "\n[TAC] Exportado a: output/programa.tac\n";
    }

    return result.success() ? 0 : 1;
}
