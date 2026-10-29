// STUB DE LA PARTE 1: ver value.h. Lo reemplaza la Parte 2.

#include "value.h"

#include <cctype>

namespace sqlint {
namespace storage {

namespace {

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && isLeap(y) ? 29 : days[m - 1];
}

}  // namespace

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

}  // namespace storage
}  // namespace sqlint
