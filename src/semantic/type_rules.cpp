#include "type_rules.h"

namespace sqlint {
namespace semantic {

using storage::SqlType;
using storage::TypeKind;

bool isNumeric(const SqlType& t) { return t.kind == TypeKind::Int || t.kind == TypeKind::Float; }

bool isNull(const SqlType& t) { return t.kind == TypeKind::Null; }

bool comparable(const SqlType& a, const SqlType& b) {
    if (isNull(a) || isNull(b)) return true;
    if (isNumeric(a) && isNumeric(b)) return true;
    return a.kind == b.kind;
}

bool arithmeticResult(const SqlType& a, const SqlType& b, SqlType& result) {
    bool aOk = isNumeric(a) || isNull(a);
    bool bOk = isNumeric(b) || isNull(b);
    if (!aOk || !bOk) return false;
    if (a.kind == TypeKind::Float || b.kind == TypeKind::Float) {
        result = SqlType{TypeKind::Float};
    } else if (isNull(a) && isNull(b)) {
        result = SqlType{TypeKind::Null};  // NULL + NULL sigue siendo NULL
    } else {
        result = SqlType{TypeKind::Int};
    }
    return true;
}

bool isBooleanLike(const SqlType& t) { return t.kind == TypeKind::Boolean || isNull(t); }

bool assignable(const SqlType& column, const SqlType& value) {
    if (isNull(value)) return true;
    if (column.kind == TypeKind::Float && value.kind == TypeKind::Int) return true;
    return column.kind == value.kind;
}

SqlType fromSpec(const ast::DataTypeSpec& spec) {
    switch (spec.name) {
        case ast::TypeName::Int: return SqlType{TypeKind::Int};
        case ast::TypeName::Float: return SqlType{TypeKind::Float};
        case ast::TypeName::Varchar: return SqlType{TypeKind::Varchar, static_cast<int>(spec.length)};
        case ast::TypeName::Boolean: return SqlType{TypeKind::Boolean};
        case ast::TypeName::Date: return SqlType{TypeKind::Date};
    }
    return SqlType{};
}

}  // namespace semantic
}  // namespace sqlint
