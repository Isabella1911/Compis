#ifndef COMPISCRIPT_CODEGEN_MIPS_EMITTER_H
#define COMPISCRIPT_CODEGEN_MIPS_EMITTER_H

// Emisor de texto MARS/MIPS32: .data, .text, etiquetas e instrucciones.
// Usa pseudoinstrucciones que MARS acepta (li, la, move, bnez, ...).

#include <sstream>
#include <string>
#include <vector>

namespace compiscript {
namespace codegen {
namespace mips {

class MipsEmitter {
public:
    void directive(const std::string& line);
    void label(const std::string& name);
    void insn(const std::string& line);
    void comment(const std::string& text);
    void blank();

    std::string str() const { return out_.str(); }

private:
    std::ostringstream out_;
};

std::string escapeAsciiz(const std::string& value);

}  // namespace mips
}  // namespace codegen
}  // namespace compiscript

#endif  // COMPISCRIPT_CODEGEN_MIPS_EMITTER_H
