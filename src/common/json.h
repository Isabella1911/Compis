#ifndef SQLINT_COMMON_JSON_H
#define SQLINT_COMMON_JSON_H

// Utilidades minimas para escribir JSON a mano. La salida --json es el unico
// canal entre sqlint y la IDE, asi que todo texto que salga ahi (mensajes,
// lexemas, valores VARCHAR) tiene que pasar por jsonString.

#include <cstdio>
#include <string>

namespace sqlint {
namespace json {

// Devuelve s entre comillas y escapado. Los bytes UTF-8 (>= 0x80) pasan tal
// cual: JSON acepta UTF-8 directo y asi la IDE muestra los acentos.
inline std::string jsonString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
    return out;
}

}  // namespace json
}  // namespace sqlint

#endif  // SQLINT_COMMON_JSON_H
