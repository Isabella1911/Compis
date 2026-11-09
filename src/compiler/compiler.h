#ifndef COMPISCRIPT_COMPILER_COMPILER_H
#define COMPISCRIPT_COMPILER_COMPILER_H

#include <string>

#include "result.h"

// Fachada publica. Fuera de src/frontend/, nadie deberia necesitar
// incluir nada de ANTLR: se le entrega una cadena de codigo fuente y
// devuelve un CompilationResult ya armado.
namespace compiscript {
namespace compiler {

struct CompileOptions {
    // Ultima etapa: TAC -> MIPS. Los errores del backend (CGxxx) se agregan
    // a diagnostics y en ese caso no hay ensamblador.
    bool mips = false;
};

// Ruta unica del compilador: ANTLR -> AST -> semantica -> TAC -> MIPS. Una
// etapa solo corre si las anteriores no reportaron errores.
class Compiler {
public:
    static CompilationResult compile(const std::string& source,
                                     const CompileOptions& options = {});
};

}  // namespace compiler
}  // namespace compiscript

#endif  // COMPISCRIPT_COMPILER_COMPILER_H
