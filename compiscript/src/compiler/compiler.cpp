#include "compiler.h"

#include "diagnostics/reporter.h"
#include "frontend/parser_driver.h"

namespace compiscript {
namespace compiler {

CompilationResult Compiler::compile(const std::string& source) {
    // Sin estado global: reporter nuevo en cada compilacion, para que un
    // contador que no se reinicia no produzca bugs que solo aparecen en la
    // IDE (donde el mismo proceso compila muchas veces) y nunca en los tests
    // (donde cada caso arranca un proceso nuevo).
    diagnostics::DiagnosticReporter reporter;

    CompilationResult result;
    result.ast = frontend::parse(source, reporter);
    result.diagnostics = reporter.sorted();
    return result;
}

}  // namespace compiler
}  // namespace compiscript
