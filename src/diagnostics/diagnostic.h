#ifndef SQLINT_DIAGNOSTICS_DIAGNOSTIC_H
#define SQLINT_DIAGNOSTICS_DIAGNOSTIC_H

// Contrato A.6 (docs/sql): la forma de todo error que produce el interprete,
// sea lexico, sintactico, semantico o de ejecucion. La IDE y las pruebas
// leen estos campos; el mensaje es para humanos, el codigo para las pruebas.

#include <stdexcept>
#include <string>
#include <utility>

namespace sqlint {
namespace diagnostics {

enum class Phase { Lexico, Sintactico, Semantico, Ejecucion };

// Posicion en el script, 1-based. line == 0 significa "sin ubicacion".
struct SourceLoc {
    int line = 0;
    int col = 0;
};

struct Diagnostic {
    Phase phase;
    std::string code;     // "SEM002", "RUN201", ...
    SourceLoc loc;
    std::string message;  // en espanol, nombra la tabla/columna concreta
    int length = 1;       // caracteres a subrayar en la IDE
};

// Nombre de la fase tal como sale en el JSON: "lexico", "sintactico", ...
const char* phaseName(Phase phase);

// Los errores de ejecucion (Partes 2 y 3) se lanzan como SqlError; la
// Session los atrapa y los agrega como un Diagnostic mas.
class SqlError : public std::runtime_error {
public:
    explicit SqlError(Diagnostic d) : std::runtime_error(d.message), diag(std::move(d)) {}

    Diagnostic diag;
};

}  // namespace diagnostics
}  // namespace sqlint

#endif  // SQLINT_DIAGNOSTICS_DIAGNOSTIC_H
