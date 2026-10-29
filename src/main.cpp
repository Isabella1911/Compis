// sqlint: interprete SQL con almacenamiento en archivos.
//
// Uso (contrato A.7 de docs/sql):
//   sqlint --db <directorio> [opciones] <archivo.sql | ->
//
//   (sin opciones)     analiza y ejecuta; resultados como tablas de texto
//   --json             toda la salida como un unico objeto JSON (lo usa la IDE)
//   --emit tokens      lista de tokens
//   --emit parse-tree  arbol sintactico de ANTLR en formato .dot
//   --no-exec          solo lexico, sintactico y semantico (no toca disco)
//
// Con "-" en lugar de archivo, el script se lee de la entrada estandar.
// Codigo de salida: 0 sin diagnosticos, 1 con al menos uno, 2 por mal uso.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "common/json.h"
#include "diagnostics/reporter.h"
#include "frontend/parser_driver.h"

using namespace sqlint;

namespace {

struct Options {
    std::string db = "databases/escuela";
    std::string emit;  // "", "tokens", "parse-tree"
    bool json = false;
    bool noExec = false;
    std::string file;
};

const char* kUsage =
    "Uso: sqlint [--db <dir>] [--json] [--emit tokens|parse-tree] [--no-exec] "
    "<archivo.sql | ->\n";

bool parseArgs(int argc, char* argv[], Options& opts, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") {
            opts.json = true;
        } else if (arg == "--no-exec") {
            opts.noExec = true;
        } else if (arg == "--db" || arg == "--emit") {
            if (i + 1 >= argc) {
                error = "Falta el valor de " + arg;
                return false;
            }
            (arg == "--db" ? opts.db : opts.emit) = argv[++i];
        } else if (arg != "-" && arg.rfind("--", 0) == 0) {
            error = "Opción desconocida: " + arg;
            return false;
        } else if (opts.file.empty()) {
            opts.file = arg;
        } else {
            error = "Sobra el argumento: " + arg;
            return false;
        }
    }
    if (opts.file.empty()) {
        error = "Falta el archivo .sql";
        return false;
    }
    if (!opts.emit.empty() && opts.emit != "tokens" && opts.emit != "parse-tree") {
        error = "Modo de --emit desconocido: " + opts.emit;
        return false;
    }
    return true;
}

bool readSource(const std::string& path, std::string& out) {
    std::stringstream ss;
    if (path == "-") {
        ss << std::cin.rdbuf();
    } else {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        ss << file.rdbuf();
    }
    out = ss.str();
    return true;
}

std::string diagnosticsToJson(const std::vector<diagnostics::Diagnostic>& diags) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < diags.size(); ++i) {
        const auto& d = diags[i];
        if (i > 0) out << ",";
        out << "\n    {\"phase\": " << json::jsonString(diagnostics::phaseName(d.phase))
            << ", \"code\": " << json::jsonString(d.code) << ", \"line\": " << d.loc.line
            << ", \"col\": " << d.loc.col << ", \"length\": " << d.length
            << ", \"message\": " << json::jsonString(d.message) << "}";
    }
    out << (diags.empty() ? "]" : "\n  ]");
    return out.str();
}

void printDiagnosticsText(const std::vector<diagnostics::Diagnostic>& diags) {
    for (const auto& d : diags) {
        std::cerr << "[" << d.code << "] " << diagnostics::phaseName(d.phase) << " "
                  << d.loc.line << ":" << d.loc.col << ": " << d.message << "\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    Options opts;
    std::string error;
    if (!parseArgs(argc, argv, opts, error)) {
        std::cerr << error << "\n" << kUsage;
        return 2;
    }

    std::string source;
    if (!readSource(opts.file, source)) {
        std::cerr << "No se pudo abrir " << opts.file << "\n";
        return 2;
    }

    diagnostics::DiagnosticReporter reporter;
    frontend::ParseOptions parseOptions;
    parseOptions.wantParseTreeDot = opts.emit == "parse-tree";
    frontend::ParseOutput parsed = frontend::parse(source, reporter, parseOptions);
    std::vector<diagnostics::Diagnostic> diags = reporter.sorted();

    if (opts.json) {
        std::cout << "{\n  \"diagnostics\": " << diagnosticsToJson(diags);
        if (opts.emit == "tokens") {
            std::cout << ",\n  \"tokens\": " << frontend::tokensToJson(parsed.tokens);
        } else if (opts.emit == "parse-tree") {
            std::cout << ",\n  \"dot\": " << json::jsonString(parsed.parseTreeDot);
        } else {
            std::cout << ",\n  \"statements\": []";
        }
        std::cout << "\n}\n";
        return diags.empty() ? 0 : 1;
    }

    printDiagnosticsText(diags);
    if (opts.emit == "tokens") {
        std::cout << frontend::formatTokens(parsed.tokens);
    } else if (opts.emit == "parse-tree") {
        std::cout << parsed.parseTreeDot;
    } else if (diags.empty()) {
        // La ejecucion llega con el AST y la Session de la Parte 3.
        std::cout << "Sin errores léxicos ni sintácticos.\n";
    }
    return diags.empty() ? 0 : 1;
}
