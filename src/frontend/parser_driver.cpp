#include "parser_driver.h"

#include "CompiscriptLexer.h"
#include "CompiscriptParser.h"
#include "antlr4-runtime.h"
#include "ast_builder.h"
#include "error_listener.h"

namespace compiscript {
namespace frontend {

ast::ProgramPtr parse(const std::string& source, diagnostics::DiagnosticReporter& reporter) {
    antlr4::ANTLRInputStream input(source);
    compiscript::CompiscriptLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    compiscript::CompiscriptParser parser(&tokens);

    DiagnosticErrorListener errorListener(reporter);
    lexer.removeErrorListeners();
    lexer.addErrorListener(&errorListener);
    parser.removeErrorListeners();
    parser.addErrorListener(&errorListener);

    CompiscriptParser::ProgramContext* tree = parser.program();
    if (reporter.has_errors()) {
        return nullptr;
    }

    AstBuilder builder;
    return builder.build(tree);
}

}  // namespace frontend
}  // namespace compiscript
