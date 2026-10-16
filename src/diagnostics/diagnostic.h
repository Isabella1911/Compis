#ifndef COMPISCRIPT_DIAGNOSTICS_DIAGNOSTIC_H
#define COMPISCRIPT_DIAGNOSTICS_DIAGNOSTIC_H

#include <string>

namespace compiscript {
namespace diagnostics {

enum class Severity { Error, Warning };

struct Diagnostic {
    Severity severity;
    std::string code;     // "SYN001", "SEM001", ...
    std::string message;
    int line = 0;
    int column = 0;
    int length = 1;        // para subrayar en el IDE
};

}  // namespace diagnostics
}  // namespace compiscript

#endif  // COMPISCRIPT_DIAGNOSTICS_DIAGNOSTIC_H
