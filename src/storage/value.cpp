#include "value.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "diagnostics/codes.h"
#include "diagnostics/diagnostic.h"

namespace sqlint {
namespace storage {

namespace {

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && isLeap(y) ? 29 : days[m - 1];
}

template <typename T>
int cmp(const T& a, const T& b) {
    return a < b ? -1 : (b < a ? 1 : 0);
}

bool isNumericKind(TypeKind k) { return k == TypeKind::Int || k == TypeKind::Float; }

double asDouble(const Value& v) {
    return v.kind == TypeKind::Int ? static_cast<double>(v.asInt()) : v.asFloat();
}

// FLOAT con la menor cantidad de digitos que, al leerse de vuelta, da
// exactamente el mismo double. 0.1 se escribe "0.1" y no "0.10000000000000001".
std::string formatFloat(double x) {
    char buf[32];
    for (int precision = 15; precision <= 17; ++precision) {
        std::snprintf(buf, sizeof(buf), "%.*g", precision, x);
        if (std::strtod(buf, nullptr) == x) break;
    }
    return buf;
}

bool needsQuotes(const std::string& s) {
    if (s.empty()) return true;  // "" es la cadena vacia; vacio sin comillas es NULL
    if (s.front() == ' ' || s.back() == ' ') return true;
    for (char c : s) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r' || c == '\t') return true;
    }
    return false;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    out += '"';
    return out;
}

[[noreturn]] void badField(const std::string& field, const SqlType& type) {
    diagnostics::Diagnostic d;
    d.phase = diagnostics::Phase::Ejecucion;
    d.code = diagnostics::codes::RUN103;
    d.message = "El valor " + field + " no es un " + typeName(type) + " válido";
    throw diagnostics::SqlError(d);
}

bool parseInt(const std::string& s, int64_t& out) {
    if (s.empty()) return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (i == s.size()) return false;
    for (size_t j = i; j < s.size(); ++j) {
        if (!std::isdigit(static_cast<unsigned char>(s[j]))) return false;
    }
    errno = 0;
    char* end = nullptr;
    long long x = std::strtoll(s.c_str(), &end, 10);
    if (errno == ERANGE) return false;
    out = x;
    return true;
}

bool parseFloat(const std::string& s, double& out) {
    if (s.empty() || std::isspace(static_cast<unsigned char>(s[0]))) return false;
    errno = 0;
    char* end = nullptr;
    double x = std::strtod(s.c_str(), &end);
    // ERANGE tambien llega con los subnormales (5e-324), que si son validos:
    // solo se rechaza el desborde a infinito.
    if (end != s.c_str() + s.size() || std::isinf(x) || std::isnan(x)) return false;
    out = x;
    return true;
}

bool equalsIgnoreCase(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i]; ++i) {
        if (std::toupper(static_cast<unsigned char>(a[i])) != b[i]) return false;
    }
    return i == a.size() && b[i] == '\0';
}

}  // namespace

int compareValues(const Value& a, const Value& b) {
    if (isNumericKind(a.kind) && isNumericKind(b.kind)) {
        if (a.kind == TypeKind::Int && b.kind == TypeKind::Int) return cmp(a.asInt(), b.asInt());
        return cmp(asDouble(a), asDouble(b));
    }
    if (a.kind != b.kind) return cmp(static_cast<int>(a.kind), static_cast<int>(b.kind));
    switch (a.kind) {
        case TypeKind::Varchar: {
            int c = a.asString().compare(b.asString());
            return c < 0 ? -1 : (c > 0 ? 1 : 0);
        }
        case TypeKind::Boolean: return cmp(a.asBool(), b.asBool());
        case TypeKind::Date: {
            const Date& x = a.asDate();
            const Date& y = b.asDate();
            if (x.y != y.y) return cmp(x.y, y.y);
            if (x.m != y.m) return cmp(x.m, y.m);
            return cmp(x.d, y.d);
        }
        default: return 0;  // Null contra Null: no deberia llegar
    }
}

std::string toCsvField(const Value& value) {
    switch (value.kind) {
        case TypeKind::Null: return "";
        case TypeKind::Int: return std::to_string(value.asInt());
        case TypeKind::Float: return formatFloat(value.asFloat());
        case TypeKind::Boolean: return value.asBool() ? "TRUE" : "FALSE";
        case TypeKind::Date: return formatDate(value.asDate());
        case TypeKind::Varchar: {
            const std::string& s = value.asString();
            return needsQuotes(s) ? quote(s) : s;
        }
    }
    return "";
}

Value fromCsvField(const std::string& field, const SqlType& type) {
    std::string text = field;
    bool quoted = false;
    if (!field.empty() && field[0] == '"') {
        if (field.size() < 2 || field.back() != '"') badField(field, type);
        text.clear();
        for (size_t i = 1; i + 1 < field.size(); ++i) {
            if (field[i] == '"') {
                if (i + 2 >= field.size() || field[i + 1] != '"') badField(field, type);
                ++i;
            }
            text += field[i];
        }
        quoted = true;
    }
    if (!quoted && text.empty()) return Value::null();

    switch (type.kind) {
        case TypeKind::Varchar: return Value::ofString(text);
        case TypeKind::Int: {
            int64_t x;
            if (!parseInt(text, x)) badField(field, type);
            return Value::ofInt(x);
        }
        case TypeKind::Float: {
            double x;
            if (!parseFloat(text, x)) badField(field, type);
            return Value::ofFloat(x);
        }
        case TypeKind::Boolean:
            if (equalsIgnoreCase(text, "TRUE")) return Value::ofBool(true);
            if (equalsIgnoreCase(text, "FALSE")) return Value::ofBool(false);
            badField(field, type);
        case TypeKind::Date: {
            Date d;
            if (!parseDate(text, d)) badField(field, type);
            return Value::ofDate(d);
        }
        case TypeKind::Null: break;
    }
    badField(field, type);
}

bool parseDate(const std::string& text, Date& out) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return false;
    for (size_t i : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) return false;
    }
    Date d{std::stoi(text.substr(0, 4)), std::stoi(text.substr(5, 2)),
           std::stoi(text.substr(8, 2))};
    if (d.y < 1 || d.m < 1 || d.m > 12 || d.d < 1 || d.d > daysInMonth(d.y, d.m)) return false;
    out = d;
    return true;
}

std::string formatDate(const Date& d) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.y, d.m, d.d);
    return buf;
}

std::string typeName(const SqlType& type) {
    switch (type.kind) {
        case TypeKind::Null: return "NULL";
        case TypeKind::Int: return "INT";
        case TypeKind::Float: return "FLOAT";
        case TypeKind::Varchar: return "VARCHAR(" + std::to_string(type.length) + ")";
        case TypeKind::Boolean: return "BOOLEAN";
        case TypeKind::Date: return "DATE";
    }
    return "?";
}

bool parseTypeName(const std::string& text, SqlType& out) {
    if (equalsIgnoreCase(text, "INT")) {
        out = {TypeKind::Int, 0};
    } else if (equalsIgnoreCase(text, "FLOAT")) {
        out = {TypeKind::Float, 0};
    } else if (equalsIgnoreCase(text, "BOOLEAN")) {
        out = {TypeKind::Boolean, 0};
    } else if (equalsIgnoreCase(text, "DATE")) {
        out = {TypeKind::Date, 0};
    } else if (text.size() > 9 && equalsIgnoreCase(text.substr(0, 8), "VARCHAR(") &&
               text.back() == ')') {
        int64_t n;
        if (!parseInt(text.substr(8, text.size() - 9), n) || n <= 0 || n > 1000000) return false;
        out = {TypeKind::Varchar, static_cast<int>(n)};
    } else {
        return false;
    }
    return true;
}

std::string displayValue(const Value& value) {
    switch (value.kind) {
        case TypeKind::Null: return "NULL";
        case TypeKind::Varchar: return "'" + value.asString() + "'";
        case TypeKind::Date: return "'" + formatDate(value.asDate()) + "'";
        default: return toCsvField(value);
    }
}

int utf8Length(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++n;  // los bytes de continuacion no cuentan
    }
    return n;
}

}  // namespace storage
}  // namespace sqlint
