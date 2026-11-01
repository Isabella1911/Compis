#include "parser_driver.h"

#include "SQLLexer.h"
#include "SQLParser.h"
#include "ListTokenSource.h"
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

// Segunda pasada, solo si la primera encontro errores sintacticos: parsea
// cada sentencia por separado, cortando la lista de tokens en cada ';'.
//
// Hace falta porque la recuperacion de ANTLR no siempre se queda dentro de
// la sentencia que fallo. Si una sentencia EMPIEZA con un token que no puede
// iniciar ninguna (`SELEC nombre ...`), el ciclo de `script` termina, ANTLR
// espera el fin del script y para recuperarse descarta TODO lo que sigue: los
// errores de las sentencias siguientes nunca se reportaban. Parseando cada
// sentencia aparte, cada error se recupera dentro de su sentencia.
//
// Cortar por ';' es seguro porque se corta la lista de TOKENS: un ';' dentro
// de una cadena o de un comentario nunca es un token SEMI.
void reportSyntaxErrorsByStatement(const std::vector<antlr4::Token*>& all,
                                   diagnostics::DiagnosticReporter& reporter) {
    std::vector<antlr4::Token*> chunk;
    // `eof`: el EOF real del archivo para el ultimo fragmento; null para los
    // demas, que reciben uno justo despues de su ultimo token. Sin esto,
    // ListTokenSource inventa un EOF con texto "EOF" (que el traductor de
    // mensajes no reconoce) y una posicion que no corresponde.
    auto parseChunk = [&](antlr4::Token* eof) {
        bool hasStatement = false;
        for (antlr4::Token* t : chunk) hasStatement |= t->getType() != SQLLexer::SEMI;
        if (hasStatement) {
            // Copias: ListTokenSource se queda con la propiedad de sus tokens.
            // Conservan linea y columna originales, asi que los diagnosticos
            // apuntan al lugar correcto del script completo.
            std::vector<std::unique_ptr<antlr4::Token>> copies;
            for (antlr4::Token* t : chunk) copies.push_back(std::make_unique<antlr4::CommonToken>(t));
            if (eof != nullptr) {
                copies.push_back(std::make_unique<antlr4::CommonToken>(eof));
            } else {
                antlr4::Token* last = chunk.back();
                auto end = std::make_unique<antlr4::CommonToken>(antlr4::Token::EOF);
                end->setLine(last->getLine());
                end->setCharPositionInLine(last->getCharPositionInLine() + last->getText().size());
                copies.push_back(std::move(end));
            }
            antlr4::ListTokenSource source(std::move(copies));
            antlr4::CommonTokenStream stream(&source);
            SQLParser parser(&stream);
            DiagnosticErrorListener listener(reporter, Phase::Sintactico);
            parser.removeErrorListeners();
            parser.addErrorListener(&listener);
            parser.script();
        }
        chunk.clear();
    };
    antlr4::Token* eof = nullptr;
    for (antlr4::Token* t : all) {
        if (t->getType() == antlr4::Token::EOF) {
            eof = t;
            break;
        }
        if (t->getChannel() != antlr4::Token::DEFAULT_CHANNEL) continue;
        chunk.push_back(t);
        if (t->getType() == SQLLexer::SEMI) parseChunk(nullptr);
    }
    parseChunk(eof);  // la ultima sentencia puede no llevar ';'
}

}  // namespace

ParseOutput parse(const std::string& source, diagnostics::DiagnosticReporter& reporter,
                  const ParseOptions& options) {
    antlr4::ANTLRInputStream input(source);
    SQLLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    SQLParser parser(&tokens);

    // Los errores sintacticos de esta primera pasada van a un reporter
    // aparte: si hay alguno se reemplazan por los de la pasada por sentencia.
    diagnostics::DiagnosticReporter firstPassSyntax;
    DiagnosticErrorListener lexerListener(reporter, Phase::Lexico);
    DiagnosticErrorListener parserListener(firstPassSyntax, Phase::Sintactico);
    lexer.removeErrorListeners();
    lexer.addErrorListener(&lexerListener);
    parser.removeErrorListeners();
    parser.addErrorListener(&parserListener);

    SQLParser::ScriptContext* tree = parser.script();

    tokens.fill();  // por si el parser se detuvo antes del final
    reportLexicalErrors(tokens.getTokens(), reporter);
    if (firstPassSyntax.hasErrors()) {
        diagnostics::DiagnosticReporter byStatement;
        reportSyntaxErrorsByStatement(tokens.getTokens(), byStatement);
        // Por seguridad: si la segunda pasada no encontrara nada (no deberia),
        // se conservan los errores de la primera.
        const auto& syntax = byStatement.hasErrors() ? byStatement : firstPassSyntax;
        for (const auto& d : syntax.diagnostics()) {
            reporter.error(d.phase, d.code, d.message, d.loc, d.length);
        }
    }

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
