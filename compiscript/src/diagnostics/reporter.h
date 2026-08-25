#ifndef COMPISCRIPT_DIAGNOSTICS_REPORTER_H
#define COMPISCRIPT_DIAGNOSTICS_REPORTER_H

#include <string>
#include <vector>

#include "diagnostic.h"

namespace compiscript {
namespace diagnostics {

// Acumula diagnosticos durante una compilacion. Una instancia nueva por
// cada Compiler::compile(): sin estado global, para que un contador que no
// se reinicia no produzca bugs que solo aparecen en la IDE y nunca en los
// tests.
class DiagnosticReporter {
public:
    void report(Severity severity, const std::string& code, const std::string& message,
                int line, int column, int length = 1);

    void error(const std::string& code, const std::string& message, int line, int column,
               int length = 1);
    void warning(const std::string& code, const std::string& message, int line, int column,
                 int length = 1);

    bool has_errors() const;
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    // Copia ordenada por (line, column). Los diagnosticos se generan en
    // orden de recorrido, no necesariamente en orden de aparicion en el
    // codigo fuente (p. ej. errores sintacticos vs. semanticos).
    std::vector<Diagnostic> sorted() const;

private:
    std::vector<Diagnostic> diagnostics_;
};

}  // namespace diagnostics
}  // namespace compiscript

#endif  // COMPISCRIPT_DIAGNOSTICS_REPORTER_H
