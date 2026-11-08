#include "mips_emitter.h"

#include <cstdio>

namespace compiscript {
namespace codegen {
namespace mips {

void MipsEmitter::directive(const std::string& line) { out_ << line << "\n"; }

void MipsEmitter::label(const std::string& name) { out_ << name << ":\n"; }

void MipsEmitter::insn(const std::string& line) { out_ << "    " << line << "\n"; }

void MipsEmitter::comment(const std::string& text) { out_ << "    # " << text << "\n"; }

void MipsEmitter::blank() { out_ << "\n"; }

std::string escapeAsciiz(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '\0': out += "\\0"; break;
            default:
                if (c < 32 || c == 127) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\x%02x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    return out;
}

}  // namespace mips
}  // namespace codegen
}  // namespace compiscript
