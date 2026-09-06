#include "compiler.h"

#include "diagnostics/reporter.h"
#include "frontend/parser_driver.h"
#include "semantic/declaration_collector.h"
#include "semantic/inheritance_resolver.h"
#include "semantic/name_resolver.h"
#include "semantic/type_checker.h"

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

    if (result.ast != nullptr) {
        semantic::DeclarationCollector collector(result.symbol_table, reporter);
        collector.run(*result.ast);

        semantic::InheritanceResolver inheritanceResolver(reporter);
        inheritanceResolver.run(*result.ast);

        semantic::NameResolver resolver(reporter);
        resolver.run(*result.ast);

        semantic::TypeChecker typeChecker(reporter);
        typeChecker.run(*result.ast);
    }

    result.diagnostics = reporter.sorted();
    return result;
}

}  // namespace compiler
}  // namespace compiscript
