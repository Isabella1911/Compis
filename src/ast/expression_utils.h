#ifndef SQLINT_AST_EXPRESSION_UTILS_H
#define SQLINT_AST_EXPRESSION_UTILS_H
#include "ast/nodes.h"

namespace sqlint::ast {
// Hijos del mismo nivel SQL; las subconsultas tienen su propio alcance.
template<class F> void forEachChild(const ast::Expr& e, F f) {
    using namespace ast;
    switch (e.kind) {
        case ExprKind::Unary: f(*static_cast<const Unary&>(e).operand); break;
        case ExprKind::Binary: {
            const auto& b = static_cast<const Binary&>(e); f(*b.left); f(*b.right); break;
        }
        case ExprKind::IsNull: f(*static_cast<const IsNull&>(e).operand); break;
        case ExprKind::InList: {
            const auto& in = static_cast<const InList&>(e); f(*in.operand);
            for (const auto& item : in.items) f(*item);
            break;
        }
        case ExprKind::InSubquery: f(*static_cast<const InSubquery&>(e).operand); break;
        case ExprKind::Aggregate: {
            const auto& a = static_cast<const Aggregate&>(e); if (a.arg) f(*a.arg); break;
        }
        default: break;
    }
}
}  // namespace sqlint::ast
#endif
