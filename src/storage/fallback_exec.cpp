#include "fallback_exec.h"

#include <cmath>

#include "diagnostics/codes.h"
#include "diagnostics/diagnostic.h"

namespace sqlint {
namespace storage {

namespace codes = diagnostics::codes;

namespace {

[[noreturn]] void fail(const char* code, diagnostics::SourceLoc loc, const std::string& message) {
    diagnostics::Diagnostic d;
    d.phase = diagnostics::Phase::Ejecucion;
    d.code = code;
    d.loc = loc;
    d.message = message;
    throw diagnostics::SqlError(d);
}

bool isNumeric(const Value& v) { return v.kind == TypeKind::Int || v.kind == TypeKind::Float; }

double num(const Value& v) {
    return v.kind == TypeKind::Int ? static_cast<double>(v.asInt()) : v.asFloat();
}

// Una cadena comparada con una fecha se lee como fecha (el semantico ya
// convierte los literales; esto cubre valores calculados).
void alignDates(Value& a, Value& b) {
    Date d;
    if (a.kind == TypeKind::Date && b.kind == TypeKind::Varchar && parseDate(b.asString(), d)) {
        b = Value::ofDate(d);
    } else if (b.kind == TypeKind::Date && a.kind == TypeKind::Varchar &&
               parseDate(a.asString(), d)) {
        a = Value::ofDate(d);
    }
}

Value literal(const ast::Literal& lit) {
    switch (lit.literalKind) {
        case ast::LiteralKind::Int: return Value::ofInt(lit.intValue);
        case ast::LiteralKind::Float: return Value::ofFloat(lit.floatValue);
        case ast::LiteralKind::String: return Value::ofString(lit.stringValue);
        case ast::LiteralKind::Bool: return Value::ofBool(lit.boolValue);
        case ast::LiteralKind::Null: return Value::null();
        case ast::LiteralKind::Date: {
            Date d;
            if (!parseDate(lit.stringValue, d)) {
                fail(codes::SEM011, lit.loc, "'" + lit.stringValue + "' no es una fecha válida");
            }
            return Value::ofDate(d);
        }
    }
    return Value::null();
}

Value arithmetic(ast::BinaryOp op, const Value& a, const Value& b, diagnostics::SourceLoc loc) {
    if (a.isNull() || b.isNull()) return Value::null();
    if (a.kind == TypeKind::Int && b.kind == TypeKind::Int) {
        int64_t x = a.asInt(), y = b.asInt();
        switch (op) {
            case ast::BinaryOp::Add: return Value::ofInt(x + y);
            case ast::BinaryOp::Sub: return Value::ofInt(x - y);
            case ast::BinaryOp::Mul: return Value::ofInt(x * y);
            case ast::BinaryOp::Div:
            case ast::BinaryOp::Mod:
                if (y == 0) fail(codes::RUN202, loc, "División entre cero");
                return Value::ofInt(op == ast::BinaryOp::Div ? x / y : x % y);
            default: break;
        }
    }
    double x = num(a), y = num(b);
    switch (op) {
        case ast::BinaryOp::Add: return Value::ofFloat(x + y);
        case ast::BinaryOp::Sub: return Value::ofFloat(x - y);
        case ast::BinaryOp::Mul: return Value::ofFloat(x * y);
        case ast::BinaryOp::Div:
        case ast::BinaryOp::Mod:
            if (y == 0) fail(codes::RUN202, loc, "División entre cero");
            return Value::ofFloat(op == ast::BinaryOp::Div ? x / y : std::fmod(x, y));
        default: break;
    }
    return Value::null();
}

Value comparison(ast::BinaryOp op, Value a, Value b) {
    if (a.isNull() || b.isNull()) return Value::null();
    alignDates(a, b);
    int c = compareValues(a, b);
    switch (op) {
        case ast::BinaryOp::Eq: return Value::ofBool(c == 0);
        case ast::BinaryOp::Neq: return Value::ofBool(c != 0);
        case ast::BinaryOp::Lt: return Value::ofBool(c < 0);
        case ast::BinaryOp::Le: return Value::ofBool(c <= 0);
        case ast::BinaryOp::Gt: return Value::ofBool(c > 0);
        case ast::BinaryOp::Ge: return Value::ofBool(c >= 0);
        default: return Value::null();
    }
}

// Logica de tres valores: NULL es "desconocido".
Value logical(ast::BinaryOp op, const Value& a, const Value& b) {
    bool isAnd = op == ast::BinaryOp::And;
    bool decisive = !isAnd;  // FALSE decide un AND, TRUE decide un OR
    if ((!a.isNull() && a.asBool() == decisive) || (!b.isNull() && b.asBool() == decisive)) {
        return Value::ofBool(decisive);
    }
    if (a.isNull() || b.isNull()) return Value::null();
    return Value::ofBool(!decisive);
}

bool isTrue(const Value& v) { return v.kind == TypeKind::Boolean && v.asBool(); }

}  // namespace

Value fallbackEval(const ast::Expr& expr, const TableDef& table, const Row* row) {
    switch (expr.kind) {
        case ast::ExprKind::Literal: return literal(static_cast<const ast::Literal&>(expr));
        case ast::ExprKind::ColumnRef: {
            const auto& ref = static_cast<const ast::ColumnRef&>(expr);
            int c = ref.binding.column >= 0 ? ref.binding.column : table.findColumn(ref.column.name);
            if (row == nullptr || c < 0 || c >= static_cast<int>(row->size())) {
                fail(codes::SEM002, ref.column.loc,
                     "No se puede usar la columna '" + ref.column.name + "' aquí");
            }
            return (*row)[c];
        }
        case ast::ExprKind::Unary: {
            const auto& u = static_cast<const ast::Unary&>(expr);
            Value v = fallbackEval(*u.operand, table, row);
            if (v.isNull()) return v;
            if (u.op == ast::UnaryOp::Not) return Value::ofBool(!v.asBool());
            if (u.op == ast::UnaryOp::Plus) return v;
            return v.kind == TypeKind::Int ? Value::ofInt(-v.asInt()) : Value::ofFloat(-v.asFloat());
        }
        case ast::ExprKind::Binary: {
            const auto& b = static_cast<const ast::Binary&>(expr);
            Value l = fallbackEval(*b.left, table, row);
            Value r = fallbackEval(*b.right, table, row);
            switch (b.op) {
                case ast::BinaryOp::And:
                case ast::BinaryOp::Or: return logical(b.op, l, r);
                case ast::BinaryOp::Add:
                case ast::BinaryOp::Sub:
                case ast::BinaryOp::Mul:
                case ast::BinaryOp::Div:
                case ast::BinaryOp::Mod:
                    if (!l.isNull() && !r.isNull() && !(isNumeric(l) && isNumeric(r))) break;
                    return arithmetic(b.op, l, r, b.opLoc);
                default: return comparison(b.op, l, r);
            }
            break;
        }
        case ast::ExprKind::IsNull: {
            const auto& n = static_cast<const ast::IsNull&>(expr);
            bool isNull = fallbackEval(*n.operand, table, row).isNull();
            return Value::ofBool(n.negated ? !isNull : isNull);
        }
        case ast::ExprKind::InList: {
            // x IN (a, b): TRUE si alguno es igual; si no, NULL si hubo
            // algun NULL; si no, FALSE. NOT IN niega ese resultado.
            const auto& in = static_cast<const ast::InList&>(expr);
            Value x = fallbackEval(*in.operand, table, row);
            Value result = Value::ofBool(false);
            if (x.isNull()) {
                result = Value::null();
            } else {
                for (const auto& item : in.items) {
                    Value eq = comparison(ast::BinaryOp::Eq, x, fallbackEval(*item, table, row));
                    if (isTrue(eq)) {
                        result = eq;
                        break;
                    }
                    if (eq.isNull()) result = Value::null();
                }
            }
            if (in.negated && !result.isNull()) result = Value::ofBool(!result.asBool());
            return result;
        }
        default: break;
    }
    fail(codes::SEM023, expr.loc,
         "Esta expresión todavía no se puede ejecutar (falta el evaluador de la Parte 3)");
}

ExecHooks fallbackHooks(TableStore& store) {
    ExecHooks hooks;
    hooks.eval = fallbackEval;
    hooks.matchingRows = [&store](const TableDef& table, const ast::Expr* where) {
        std::vector<StoredRow> rows = store.scan(table);
        if (where == nullptr) return rows;
        std::vector<StoredRow> out;
        for (auto& row : rows) {
            if (isTrue(fallbackEval(*where, table, &row.values))) out.push_back(std::move(row));
        }
        return out;
    };
    return hooks;
}

}  // namespace storage
}  // namespace sqlint
