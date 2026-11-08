#ifndef COMPISCRIPT_CODEGEN_MIPS_RESULT_H
#define COMPISCRIPT_CODEGEN_MIPS_RESULT_H

#include <string>
#include <vector>

#include "diagnostics/diagnostic.h"

namespace compiscript {
namespace codegen {
namespace mips {

struct MipsResult {
    bool success = false;
    std::string assembly;  // vacio si !success
    std::vector<diagnostics::Diagnostic> diagnostics;
};

}  // namespace mips
}  // namespace codegen
}  // namespace compiscript

#endif  // COMPISCRIPT_CODEGEN_MIPS_RESULT_H
