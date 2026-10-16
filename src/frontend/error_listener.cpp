#include "error_listener.h"

#include "diagnostics/codes.h"

namespace compiscript {
namespace frontend {

void DiagnosticErrorListener::syntaxError(antlr4::Recognizer* /*recognizer*/,
                                           antlr4::Token* /*offendingSymbol*/, size_t line,
                                           size_t charPositionInLine, const std::string& msg,
                                           std::exception_ptr /*e*/) {
    reporter_.error(diagnostics::codes::SYN001, msg, static_cast<int>(line),
                     static_cast<int>(charPositionInLine) + 1);
}

}  // namespace frontend
}  // namespace compiscript
