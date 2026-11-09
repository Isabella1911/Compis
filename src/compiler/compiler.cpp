#include "compiler.h"

#include "codegen/frame_layout.h"
#include "codegen/mips/mips_generator.h"
#include "codegen/tac_generator.h"
#include "diagnostics/codes.h"
#include "diagnostics/reporter.h"
#include "frontend/parser_driver.h"
#include "semantic/closure_analyzer.h"
#include "semantic/control_flow_checker.h"
#include "semantic/declaration_collector.h"
#include "semantic/inheritance_resolver.h"
#include "semantic/name_resolver.h"
#include "semantic/type_checker.h"

namespace compiscript {
namespace compiler {

CompilationResult Compiler::compile(const std::string& source, const CompileOptions& options) {
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

        semantic::ControlFlowChecker controlFlowChecker(reporter);
        controlFlowChecker.run(*result.ast);

        semantic::ClosureAnalyzer closureAnalyzer;
        closureAnalyzer.run(*result.ast);

        if (!reporter.has_errors()) {
            codegen::FrameLayout frameLayout;
            frameLayout.run(*result.ast, result.symbol_table);

            codegen::TacGenerator generator;
            result.tac = generator.generate(*result.ast, frameLayout);
            // Algo que el generador no supo traducir: no se publica un TAC
            // incompleto (y por lo tanto tampoco MIPS).
            for (const auto& what : generator.unsupportedConstructs()) {
                reporter.error(diagnostics::codes::CG004,
                               "El generador de codigo no soporta: " + what, 0, 0);
            }
            result.has_tac = generator.unsupportedConstructs().empty();
        }
    }

    result.diagnostics = reporter.sorted();

    if (options.mips && result.has_tac && result.success()) {
        codegen::mips::MipsResult mips = codegen::mips::generateMips(result.tac);
        if (mips.success) {
            result.mips = std::move(mips.assembly);
            result.has_mips = true;
        } else {
            result.diagnostics.insert(result.diagnostics.end(), mips.diagnostics.begin(),
                                      mips.diagnostics.end());
        }
    }
    return result;
}

}  // namespace compiler
}  // namespace compiscript
