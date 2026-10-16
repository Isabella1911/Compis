#ifndef COMPISCRIPT_COMPILER_RESULT_H
#define COMPISCRIPT_COMPILER_RESULT_H

#include <algorithm>
#include <vector>

#include "ast/nodes.h"
#include "diagnostics/diagnostic.h"
#include "semantic/symbol_table.h"

namespace compiscript {
namespace compiler {

struct CompilationResult {
    ast::ProgramPtr ast;  // null si hubo error sintactico
    std::vector<diagnostics::Diagnostic> diagnostics;
    // Siempre valido (nunca null): si hubo error sintactico simplemente
    // queda con el scope global vacio, sin necesidad de que el que
    // consume CompilationResult tenga que chequear null primero.
    semantic::SymbolTable symbol_table;

    bool success() const {
        return std::none_of(diagnostics.begin(), diagnostics.end(),
                             [](const diagnostics::Diagnostic& d) {
                                 return d.severity == diagnostics::Severity::Error;
                             });
    }
};

}  // namespace compiler
}  // namespace compiscript

#endif  // COMPISCRIPT_COMPILER_RESULT_H
