#ifndef SQLINT_FRONTEND_ERROR_LISTENER_H
#define SQLINT_FRONTEND_ERROR_LISTENER_H

#include <string>

#include "BaseErrorListener.h"
#include "diagnostics/reporter.h"

namespace sqlint {
namespace frontend {

// Reemplaza el ConsoleErrorListener de ANTLR: sin esto los errores salen por
// stderr en ingles y la IDE nunca los ve. Aqui se convierten en Diagnostic
// con el mensaje traducido.
//
// Los errores del parser son SYN001. El lexer normalmente no reporta nada
// (ERROR_CHAR y UNTERMINATED_STRING atrapan todo lo invalido, ver SQL.g4),
// pero si llegara a hacerlo se registra como LEX001.
class DiagnosticErrorListener : public antlr4::BaseErrorListener {
public:
    DiagnosticErrorListener(diagnostics::DiagnosticReporter& reporter, diagnostics::Phase phase)
        : reporter_(reporter), phase_(phase) {}

    void syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                     size_t charPositionInLine, const std::string& msg,
                     std::exception_ptr e) override;

private:
    diagnostics::DiagnosticReporter& reporter_;
    diagnostics::Phase phase_;
};

// Traduce al espanol los mensajes tipicos de ANTLR ("mismatched input ...",
// "missing ... at ...", etc.). Lo que no reconoce lo devuelve igual. Expuesta
// para poder probarla sin correr el parser.
std::string translateAntlrMessage(const std::string& msg);

}  // namespace frontend
}  // namespace sqlint

#endif  // SQLINT_FRONTEND_ERROR_LISTENER_H
