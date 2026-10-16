#ifndef COMPISCRIPT_COMPILER_COMPILER_H
#define COMPISCRIPT_COMPILER_COMPILER_H

#include <string>

#include "result.h"

// Fachada publica. Fuera de src/frontend/, nadie deberia necesitar
// incluir nada de ANTLR: se le entrega una cadena de codigo fuente y
// devuelve un CompilationResult ya armado.
namespace compiscript {
namespace compiler {

class Compiler {
public:
    static CompilationResult compile(const std::string& source);
};

}  // namespace compiler
}  // namespace compiscript

#endif  // COMPISCRIPT_COMPILER_COMPILER_H
