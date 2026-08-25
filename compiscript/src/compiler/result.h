#ifndef COMPISCRIPT_COMPILER_RESULT_H
#define COMPISCRIPT_COMPILER_RESULT_H

#include <algorithm>
#include <vector>

#include "ast/nodes.h"
#include "diagnostics/diagnostic.h"

namespace compiscript {
namespace compiler {

// symbol_table llega en la Etapa 2: por ahora ni se declara el campo,
// para no tener un puntero muerto sin uso durante toda esta etapa.
struct CompilationResult {
    ast::ProgramPtr ast;  // null si hubo error sintactico
    std::vector<diagnostics::Diagnostic> diagnostics;

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
