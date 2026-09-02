#ifndef COMPISCRIPT_FRONTEND_ERROR_LISTENER_H
#define COMPISCRIPT_FRONTEND_ERROR_LISTENER_H

#include "BaseErrorListener.h"
#include "diagnostics/reporter.h"

namespace compiscript {
namespace frontend {

// Reemplaza el ConsoleErrorListener por defecto de ANTLR. Sin esto, los
// errores sintacticos salen por stderr y la IDE nunca los ve: quedan
// atrapados como Diagnostic en el DiagnosticReporter en su lugar.
class DiagnosticErrorListener : public antlr4::BaseErrorListener {
public:
    explicit DiagnosticErrorListener(diagnostics::DiagnosticReporter& reporter)
        : reporter_(reporter) {}

    void syntaxError(antlr4::Recognizer* recognizer, antlr4::Token* offendingSymbol, size_t line,
                      size_t charPositionInLine, const std::string& msg,
                      std::exception_ptr e) override;

private:
    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace frontend
}  // namespace compiscript

#endif  // COMPISCRIPT_FRONTEND_ERROR_LISTENER_H
