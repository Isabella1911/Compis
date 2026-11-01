#include "evaluator.h"
#include "subquery.h"
#include <limits>
#include <stdexcept>

#include <cmath>

#include "diagnostics/codes.h"
#include "diagnostics/diagnostic.h"

namespace sqlint {
namespace exec {
using namespace storage;

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
            case ast::BinaryOp::Add:
            case ast::BinaryOp::Sub:
            case ast::BinaryOp::Mul: {
                int64_t result;
                bool overflow = op == ast::BinaryOp::Add ? __builtin_add_overflow(x, y, &result)
                    : op == ast::BinaryOp::Sub ? __builtin_sub_overflow(x, y, &result)
                    : __builtin_mul_overflow(x, y, &result);
                if (overflow) fail(codes::SEM007, loc, "El resultado no cabe en un INT (64 bits)");
                return Value::ofInt(result);
            }
            case ast::BinaryOp::Div:
            case ast::BinaryOp::Mod:
                if (y == 0) fail(codes::RUN202, loc, "División entre cero");
                if (x == std::numeric_limits<int64_t>::min() && y == -1) {
                    if (op == ast::BinaryOp::Mod) return Value::ofInt(0);
                    fail(codes::SEM007, loc, "El resultado no cabe en un INT (64 bits)");
                }
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

}  // namespace

bool isTrue(const Value& v) { return v.kind == TypeKind::Boolean && v.asBool(); }

int compareNullable(const Value& a, const Value& b) {
    if (a.isNull() || b.isNull()) return a.isNull() == b.isNull() ? 0 : (a.isNull() ? 1 : -1);
    return compareValues(a, b);
}

Value equalValues(Value a, Value b) { return comparison(ast::BinaryOp::Eq, std::move(a), std::move(b)); }

Value eval(const ast::Expr& expr, const EvalContext& ctx) {
    switch (expr.kind) {
        case ast::ExprKind::Literal: return literal(static_cast<const ast::Literal&>(expr));
        case ast::ExprKind::ColumnRef: {
            const auto& ref = static_cast<const ast::ColumnRef&>(expr);
            const auto& b = ref.binding;
            if (b.outputColumn >= 0) {
                if (!ctx.output) throw std::logic_error("Alias de ORDER BY sin proyección");
                return ctx.output->at(b.outputColumn);
            }
            const EvalContext* source = &ctx;
            for (int d = 0; d < b.depth && source; ++d) source = source->outer;
            if (b.depth < 0 || b.source < 0 || b.column < 0 || !source ||
                b.source >= static_cast<int>(source->rows.size()) || !source->rows[b.source]) {
                throw std::logic_error("ColumnRef sin binding semántico válido: " + ref.column.name);
            }
            return source->rows[b.source]->at(b.column);
        }
        case ast::ExprKind::Unary: {
            const auto& u = static_cast<const ast::Unary&>(expr);
            Value v = eval(*u.operand, ctx);
            if (v.isNull()) return v;
            if (u.op == ast::UnaryOp::Not) return Value::ofBool(!v.asBool());
            if (u.op == ast::UnaryOp::Plus) return v;
            if (v.kind == TypeKind::Int && v.asInt() == std::numeric_limits<int64_t>::min())
                fail(codes::SEM007, u.loc, "El resultado no cabe en un INT (64 bits)");
            return v.kind == TypeKind::Int ? Value::ofInt(-v.asInt()) : Value::ofFloat(-v.asFloat());
        }
        case ast::ExprKind::Binary: {
            const auto& b = static_cast<const ast::Binary&>(expr);
            Value l = eval(*b.left, ctx);
            Value r = eval(*b.right, ctx);
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
            bool isNull = eval(*n.operand, ctx).isNull();
            return Value::ofBool(n.negated ? !isNull : isNull);
        }
        case ast::ExprKind::InList: {
            // x IN (a, b): TRUE si alguno es igual; si no, NULL si hubo
            // algun NULL; si no, FALSE. NOT IN niega ese resultado.
            const auto& in = static_cast<const ast::InList&>(expr);
            Value x = eval(*in.operand, ctx);
            Value result = Value::ofBool(false);
            if (x.isNull()) {
                result = Value::null();
            } else {
                for (const auto& item : in.items) {
                    Value eq = comparison(ast::BinaryOp::Eq, x, eval(*item, ctx));
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
        case ast::ExprKind::Aggregate: {
            const auto* agg = &static_cast<const ast::Aggregate&>(expr);
            if (!ctx.aggregates) throw std::logic_error("Agregado fuera de un grupo");
            return ctx.aggregates->at(agg);
        }
        case ast::ExprKind::Exists: {
            const auto& e = static_cast<const ast::Exists&>(expr);
            bool exists = !runSubquery(*e.subquery, ctx, true).rows.empty();
            return Value::ofBool(e.negated ? !exists : exists);
        }
        case ast::ExprKind::ScalarSubquery: {
            const auto& e = static_cast<const ast::ScalarSubquery&>(expr);
            auto result = runSubquery(*e.subquery, ctx);
            if (result.rows.size() > 1) {
                fail(codes::RUN201, e.loc, "La subconsulta escalar devolvió más de una fila");
            }
            return result.rows.empty() ? Value::null() : result.rows.front().at(0);
        }
        case ast::ExprKind::InSubquery: {
            const auto& in = static_cast<const ast::InSubquery&>(expr);
            Value x = eval(*in.operand, ctx);
            auto rows = runSubquery(*in.subquery, ctx).rows;
            Value result = Value::ofBool(false);
            // Incluso NULL IN (conjunto vacío) es FALSE.
            for (const auto& row : rows) {
                Value eq = equalValues(x, row.at(0));
                if (isTrue(eq)) { result = eq; break; }
                if (eq.isNull()) result = Value::null();
            }
            if (in.negated && !result.isNull()) result = Value::ofBool(!result.asBool());
            return result;
        }
    }
    throw std::logic_error("Expresión no implementada en el evaluador");
}

}  // namespace exec
}  // namespace sqlint
