#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ast/printer.h"
#include "compiler/compiler.h"
#include "diagnostics/diagnostic.h"
#include "ir/tac_printer.h"
#include "semantic/printer.h"

namespace fs = std::filesystem;

static std::string readFile(const std::string& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        throw std::runtime_error("No existe el archivo " + path);
    }
    // Un directorio se abre "bien" con ifstream y se lee vacio: sin esta
    // revision se compilaria como un programa vacio.
    if (!fs::is_regular_file(path, ec)) {
        throw std::runtime_error(path + " no es un archivo");
    }
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("No se pudo abrir " + path + " (permisos?)");
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
//   compiscript <archivo.cps>              salida completa (la usa la IDE):
//                                          diagnosticos, AST, tabla de simbolos,
//                                          TAC y output/programa.asm
//   compiscript --tac <archivo.cps>        solo TAC (o diagnosticos)
//   compiscript --mips <archivo.cps> [-o]  solo ensamblador MIPS (o diagnosticos)
// Si hay cualquier error no queda ningun .asm: tampoco uno viejo de una
// compilacion anterior, para que nadie lo confunda con el actual.
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

    if (onlyTac && onlyMips) {
        std::cerr << "Error: --tac y --mips no se pueden usar juntos\n";
        usage(argv[0]);
        return 2;
    }
    if (path.empty()) {
        std::cerr << "Error: falta el archivo .cps\n";
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
        // Tampoco aqui puede quedar el .asm de un intento anterior.
        if (!outAsm.empty()) {
            std::error_code ec;
            fs::remove(outAsm, ec);
        }
        return 1;
    }

    using namespace compiscript;
    compiler::CompileOptions options;
    options.mips = !onlyTac;
    compiler::CompilationResult result = compiler::Compiler::compile(source, options);

    if (onlyTac) {
        if (result.has_tac) {
            std::cout << ir::printProgram(result.tac);
        } else {
            printDiagnostics(result);
        }
        return result.success() ? 0 : 1;
    }

    if (onlyMips) {
        if (!result.has_mips) {
            printDiagnostics(result.diagnostics, std::cerr);
            if (!outAsm.empty()) {
                std::error_code ec;
                fs::remove(outAsm, ec);
            }
            return 1;
        }

        if (outAsm.empty()) {
            std::cout << result.mips;
        } else {
            std::ofstream out(outAsm, std::ios::binary | std::ios::trunc);
            if (!out) {
                std::cerr << "Error: no se pudo escribir " << outAsm << "\n";
                return 1;
            }
            out << result.mips;
            if (!out) {
                std::cerr << "Error: fallo al escribir " << outAsm << "\n";
                fs::remove(outAsm);
                return 1;
            }
        }
        return 0;
    }

    printDiagnostics(result);

    // El .asm de una compilacion anterior se borra siempre al empezar: si esta
    // falla (incluso por un error sintactico, que corta antes), la IDE no
    // debe mostrar un ensamblador que ya no corresponde al fuente.
    const char* asmPath = "output/programa.asm";
    {
        std::error_code ec;
        fs::remove(asmPath, ec);
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

    if (result.has_tac) {
        std::string tac = ir::printProgram(result.tac);
        std::cout << "\nCodigo intermedio:\n" << tac;
        std::ofstream tacFile("output/programa.tac");
        tacFile << tac;
        std::cout << "\n[TAC] Exportado a: output/programa.tac\n";
    }

    // Ensamblador para la IDE (pestana MIPS).
    if (result.has_mips) {
        std::ofstream asmFile(asmPath, std::ios::binary | std::ios::trunc);
        asmFile << result.mips;
        std::cout << "\n[MIPS] Exportado a: " << asmPath << "\n";
    }

    return result.success() ? 0 : 1;
}
