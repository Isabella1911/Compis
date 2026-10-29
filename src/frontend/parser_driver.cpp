#include "parser_driver.h"

#include "SQLLexer.h"
#include "SQLParser.h"
#include "antlr4-runtime.h"
#include "ast_builder.h"
#include "diagnostics/codes.h"
#include "error_listener.h"
#include "parse_tree_dot.h"

namespace sqlint {
namespace frontend {

namespace {

using diagnostics::Phase;

// Los tokens de error viven en el canal oculto (ver SQL.g4), asi que el
// parser nunca los vio. Aqui se convierten en LEX001/LEX002.
void reportLexicalErrors(const std::vector<antlr4::Token*>& tokens,
                         diagnostics::DiagnosticReporter& reporter) {
    for (antlr4::Token* t : tokens) {
        diagnostics::SourceLoc loc{static_cast<int>(t->getLine()),
                                   static_cast<int>(t->getCharPositionInLine()) + 1};
        if (t->getType() == SQLLexer::ERROR_CHAR) {
            reporter.error(Phase::Lexico, diagnostics::codes::LEX001,
                           "Carácter no reconocido '" + t->getText() + "'", loc);
        } else if (t->getType() == SQLLexer::UNTERMINATED_STRING) {
            reporter.error(Phase::Lexico, diagnostics::codes::LEX002,
                           "Cadena sin cerrar: falta la comilla simple (') de cierre", loc,
                           static_cast<int>(t->getText().size()));
        }
    }
}

bool isErrorToken(size_t type) {
    return type == SQLLexer::ERROR_CHAR || type == SQLLexer::UNTERMINATED_STRING;
}

std::vector<TokenInfo> collectTokens(const std::vector<antlr4::Token*>& tokens,
                                     const antlr4::dfa::Vocabulary& vocabulary) {
    std::vector<TokenInfo> out;
    for (antlr4::Token* t : tokens) {
        if (t->getType() == antlr4::Token::EOF) continue;
        bool visible = t->getChannel() == antlr4::Token::DEFAULT_CHANNEL;
        if (!visible && !isErrorToken(t->getType())) continue;  // comentarios
        out.push_back(TokenInfo{std::string(vocabulary.getSymbolicName(t->getType())),
                                t->getText(), static_cast<int>(t->getLine()),
                                static_cast<int>(t->getCharPositionInLine()) + 1});
    }
    return out;
}

}  // namespace

ParseOutput parse(const std::string& source, diagnostics::DiagnosticReporter& reporter,
                  const ParseOptions& options) {
    antlr4::ANTLRInputStream input(source);
    SQLLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    SQLParser parser(&tokens);

    DiagnosticErrorListener lexerListener(reporter, Phase::Lexico);
    DiagnosticErrorListener parserListener(reporter, Phase::Sintactico);
    lexer.removeErrorListeners();
    lexer.addErrorListener(&lexerListener);
    parser.removeErrorListeners();
    parser.addErrorListener(&parserListener);

    SQLParser::ScriptContext* tree = parser.script();

    tokens.fill();  // por si el parser se detuvo antes del final
    reportLexicalErrors(tokens.getTokens(), reporter);

    ParseOutput out;
    out.tokens = collectTokens(tokens.getTokens(), lexer.getVocabulary());
    if (options.wantParseTreeDot) {
        out.parseTreeDot = parseTreeToDot(tree, parser);
    }
    if (!reporter.hasErrors()) {
        out.script = AstBuilder().build(tree);
    }
    return out;
}

}  // namespace frontend
}  // namespace sqlint
