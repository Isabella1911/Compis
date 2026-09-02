#include "reporter.h"

#include <algorithm>

namespace compiscript {
namespace diagnostics {

void DiagnosticReporter::report(Severity severity, const std::string& code,
                                 const std::string& message, int line, int column, int length) {
    diagnostics_.push_back(Diagnostic{severity, code, message, line, column, length});
}

void DiagnosticReporter::error(const std::string& code, const std::string& message, int line,
                                int column, int length) {
    report(Severity::Error, code, message, line, column, length);
}

void DiagnosticReporter::warning(const std::string& code, const std::string& message, int line,
                                  int column, int length) {
    report(Severity::Warning, code, message, line, column, length);
}

bool DiagnosticReporter::has_errors() const {
    return std::any_of(diagnostics_.begin(), diagnostics_.end(),
                        [](const Diagnostic& d) { return d.severity == Severity::Error; });
}

std::vector<Diagnostic> DiagnosticReporter::sorted() const {
    std::vector<Diagnostic> copy = diagnostics_;
    std::stable_sort(copy.begin(), copy.end(), [](const Diagnostic& a, const Diagnostic& b) {
        if (a.line != b.line) return a.line < b.line;
        return a.column < b.column;
    });
    return copy;
}

}  // namespace diagnostics
}  // namespace compiscript
