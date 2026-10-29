#include "reporter.h"

#include <algorithm>

namespace sqlint {
namespace diagnostics {

const char* phaseName(Phase phase) {
    switch (phase) {
        case Phase::Lexico: return "lexico";
        case Phase::Sintactico: return "sintactico";
        case Phase::Semantico: return "semantico";
        case Phase::Ejecucion: return "ejecucion";
    }
    return "desconocida";
}

void DiagnosticReporter::error(Phase phase, const std::string& code, const std::string& message,
                               SourceLoc loc, int length) {
    diagnostics_.push_back(Diagnostic{phase, code, loc, message, length});
}

bool DiagnosticReporter::hasErrorsIn(Phase phase) const {
    return std::any_of(diagnostics_.begin(), diagnostics_.end(),
                       [phase](const Diagnostic& d) { return d.phase == phase; });
}

std::vector<Diagnostic> DiagnosticReporter::sorted() const {
    std::vector<Diagnostic> copy = diagnostics_;
    std::stable_sort(copy.begin(), copy.end(), [](const Diagnostic& a, const Diagnostic& b) {
        if (a.loc.line != b.loc.line) return a.loc.line < b.loc.line;
        return a.loc.col < b.loc.col;
    });
    return copy;
}

}  // namespace diagnostics
}  // namespace sqlint
