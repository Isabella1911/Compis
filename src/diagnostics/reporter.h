#ifndef SQLINT_DIAGNOSTICS_REPORTER_H
#define SQLINT_DIAGNOSTICS_REPORTER_H

#include <string>
#include <vector>

#include "diagnostic.h"

namespace sqlint {
namespace diagnostics {

// Acumula diagnosticos durante una ejecucion de sqlint. Una instancia nueva
// por script: sin estado global, para que la IDE y las pruebas vean lo mismo.
class DiagnosticReporter {
public:
    void error(Phase phase, const std::string& code, const std::string& message, SourceLoc loc,
               int length = 1);
    void add(const Diagnostic& d) { diagnostics_.push_back(d); }

    bool hasErrors() const { return !diagnostics_.empty(); }
    bool hasErrorsIn(Phase phase) const;
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    // Copia ordenada por (linea, columna). Los errores lexicos se detectan
    // despues de los sintacticos (ver parser_driver), asi que el orden de
    // insercion no coincide con el del script.
    std::vector<Diagnostic> sorted() const;

private:
    std::vector<Diagnostic> diagnostics_;
};

}  // namespace diagnostics
}  // namespace sqlint

#endif  // SQLINT_DIAGNOSTICS_REPORTER_H
