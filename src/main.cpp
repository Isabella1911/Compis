// sqlint: interprete SQL con almacenamiento en archivos.
//
// Uso (contrato A.7 de docs/sql):
//   sqlint --db <directorio> [opciones] <archivo.sql | ->
//
//   (sin opciones)     analiza y ejecuta; resultados como tablas de texto
//   --json             toda la salida como un unico objeto JSON (lo usa la IDE)
//   --emit tokens      lista de tokens
//   --emit parse-tree  arbol sintactico de ANTLR en formato .dot
//   --emit ast         AST como arbol de texto indentado
//   --emit ast-dot     AST en formato .dot
//   --no-exec          solo lexico, sintactico y semantico (no toca disco)
//
// Con "-" en lugar de archivo, el script se lee de la entrada estandar.
// Codigo de salida: 0 sin diagnosticos, 1 con al menos uno, 2 por mal uso.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "ast/printer.h"
#include "common/json.h"
#include "diagnostics/reporter.h"
#include "frontend/parser_driver.h"
#include "exec/session.h"
#include "exec/result_json.h"
#include "storage/catalog.h"

using namespace sqlint;

namespace {

struct Options {
    std::string db = "databases/escuela";
    std::string emit;  // "", "tokens", "parse-tree", "ast", "ast-dot"
    bool json = false;
    bool noExec = false;
    std::string file;
};

const char* kUsage =
    "Uso: sqlint [--db <dir>] [--json] [--emit tokens|parse-tree|ast|ast-dot|catalog] [--no-exec] "
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
    if (opts.file.empty() && opts.emit != "catalog") {
        error = "Falta el archivo .sql";
        return false;
    }
    if (!opts.emit.empty() && opts.emit != "tokens" && opts.emit != "parse-tree" &&
        opts.emit != "ast" && opts.emit != "ast-dot" && opts.emit != "catalog") {
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
    if (opts.emit != "catalog" && !readSource(opts.file, source)) {
        std::cerr << "No se pudo abrir " << opts.file << "\n";
        return 2;
    }

    diagnostics::DiagnosticReporter reporter;
    frontend::ParseOptions parseOptions;
    parseOptions.wantParseTreeDot = opts.emit == "parse-tree";
    frontend::ParseOutput parsed = frontend::parse(source, reporter, parseOptions);
    std::vector<diagnostics::Diagnostic> diags = reporter.sorted();

    exec::RunOutput execution;
    std::string catalogJson;
    if ((parsed.script && opts.emit.empty()) || opts.emit == "catalog") {
        try {
            if (opts.emit == "catalog") catalogJson = storage::Catalog::load(opts.db).toJson();
            else {
                exec::Session session(opts.db);
                execution = session.run(*parsed.script, opts.noExec);
                diags.insert(diags.end(), execution.diagnostics.begin(), execution.diagnostics.end());
            }
        } catch (const diagnostics::SqlError& e) {
            diags.push_back(e.diag);
        }
    }

    // Sin AST (hubo errores) las vistas del AST quedan vacias.
    std::string astText;
    if (parsed.script && opts.emit == "ast") astText = ast::printTree(*parsed.script);
    if (parsed.script && opts.emit == "ast-dot") astText = ast::toDot(*parsed.script);

    if (opts.json) {
        std::cout << "{\n  \"diagnostics\": " << exec::diagnosticsToJson(diags);
        if (opts.emit == "tokens") {
            std::cout << ",\n  \"tokens\": " << frontend::tokensToJson(parsed.tokens);
        } else if (opts.emit == "parse-tree") {
            std::cout << ",\n  \"dot\": " << json::jsonString(parsed.parseTreeDot);
        } else if (opts.emit == "ast" || opts.emit == "ast-dot") {
            const char* key = opts.emit == "ast" ? "ast" : "dot";
            std::cout << ",\n  \"" << key << "\": "
                      << (parsed.script ? json::jsonString(astText) : "null");
        } else if (opts.emit == "catalog") {
            std::cout << ",\n  \"catalog\": " << (catalogJson.empty() ? "null" : catalogJson);
        } else {
            std::cout << ",\n  \"statements\": " << exec::statementsToJson(execution.statements);
        }
        std::cout << "\n}\n";
        return diags.empty() ? 0 : 1;
    }

    printDiagnosticsText(diags);
    if (opts.emit == "tokens") {
        std::cout << frontend::formatTokens(parsed.tokens);
    } else if (opts.emit == "parse-tree") {
        std::cout << parsed.parseTreeDot;
    } else if (opts.emit == "ast" || opts.emit == "ast-dot") {
        std::cout << astText;
    } else if (opts.emit == "catalog") {
        if (!catalogJson.empty()) std::cout << catalogJson << "\n";
    } else {
        for (const auto& result : execution.statements) std::cout << exec::formatTable(result);
        if (opts.noExec && diags.empty()) std::cout << "Sin errores semánticos.\n";
    }
    return diags.empty() ? 0 : 1;
}
