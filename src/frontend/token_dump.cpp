#include "token_dump.h"

#include <algorithm>
#include <sstream>

#include "common/json.h"

namespace sqlint {
namespace frontend {

namespace {

// Los saltos de linea dentro de una cadena desalinearian la tabla.
std::string printable(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out += c;
        }
    }
    return out;
}

// Ancho visible: los bytes de continuacion UTF-8 (10xxxxxx) no ocupan columna.
size_t visibleWidth(const std::string& s) {
    return std::count_if(s.begin(), s.end(),
                         [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; });
}

std::string pad(const std::string& s, size_t width) {
    size_t w = visibleWidth(s);
    return w >= width ? s : s + std::string(width - w, ' ');
}

}  // namespace

std::string formatTokens(const std::vector<TokenInfo>& tokens) {
    size_t typeWidth = 4;   // "TIPO"
    size_t textWidth = 6;   // "LEXEMA"
    for (const auto& t : tokens) {
        typeWidth = std::max(typeWidth, t.type.size());
        textWidth = std::max(textWidth, visibleWidth(printable(t.text)));
    }

    std::ostringstream out;
    out << pad("TIPO", typeWidth) << "  " << pad("LEXEMA", textWidth) << "  LINEA:COL\n";
    for (const auto& t : tokens) {
        out << pad(t.type, typeWidth) << "  " << pad(printable(t.text), textWidth) << "  "
            << t.line << ":" << t.col << "\n";
    }
    return out.str();
}

std::string tokensToJson(const std::vector<TokenInfo>& tokens) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        if (i > 0) out << ",";
        out << "\n    {\"type\": " << json::jsonString(t.type)
            << ", \"text\": " << json::jsonString(t.text) << ", \"line\": " << t.line
            << ", \"col\": " << t.col << "}";
    }
    out << (tokens.empty() ? "]" : "\n  ]");
    return out.str();
}

}  // namespace frontend
}  // namespace sqlint
