#include "type.h"

#include "diagnostics/codes.h"
#include "scope.h"
#include "symbol.h"

namespace compiscript {
namespace semantic {

bool Type::equals(const Type& other) const {
    if (kind == TypeKind::Error || other.kind == TypeKind::Error) return true;

    // null es compatible con tipos de referencia (clase, arreglo, funcion)
    // en cualquiera de los dos lados, pero no con los tipos primitivos
    // (integer/string/boolean): asignar `null` a un `Perro` tiene sentido,
    // asignarlo a un `integer` no.
    auto isReference = [](TypeKind k) {
        return k == TypeKind::Class || k == TypeKind::Array || k == TypeKind::Function;
    };
    if (kind == TypeKind::Null && isReference(other.kind)) return true;
    if (other.kind == TypeKind::Null && isReference(kind)) return true;

    // [] se puede contextualizar como cualquier arreglo sin usar Error como comodin.
    if (kind == TypeKind::EmptyElement || other.kind == TypeKind::EmptyElement) return true;
    if (kind != other.kind) return false;

    switch (kind) {
        case TypeKind::Array:
            if (!element_type || !other.element_type) return false;
            return element_type->equals(*other.element_type);
        case TypeKind::Class:
            return class_symbol == other.class_symbol;
        case TypeKind::Function: {
            if (param_types.size() != other.param_types.size()) return false;
            for (size_t i = 0; i < param_types.size(); i++) {
                if (!param_types[i] || !other.param_types[i]) return false;
                if (!param_types[i]->equals(*other.param_types[i])) return false;
            }
            if (!return_type || !other.return_type) return false;
            return return_type->equals(*other.return_type);
        }
        default:
            return true;  // primitivos: mismo TypeKind ya alcanza
    }
}

namespace {
TypePtr makeSimple(TypeKind kind) {
    auto t = std::make_shared<Type>();
    t->kind = kind;
    return t;
}
}  // namespace

TypePtr makeIntegerType() { return makeSimple(TypeKind::Integer); }
TypePtr makeStringType() { return makeSimple(TypeKind::String); }
TypePtr makeBooleanType() { return makeSimple(TypeKind::Boolean); }
TypePtr makeNullType() { return makeSimple(TypeKind::Null); }
TypePtr makeVoidType() { return makeSimple(TypeKind::Void); }
TypePtr makeErrorType() { return makeSimple(TypeKind::Error); }
TypePtr makeEmptyElementType() { return makeSimple(TypeKind::EmptyElement); }

TypePtr commonType(const TypePtr& left, const TypePtr& right) {
    if (!left) return right;
    if (!right) return left;
    if (left->kind == TypeKind::EmptyElement || left->kind == TypeKind::Null) return right;
    if (right->kind == TypeKind::EmptyElement || right->kind == TypeKind::Null) return left;
    if (left->kind == TypeKind::Array && right->kind == TypeKind::Array)
        return makeArrayType(commonType(left->element_type, right->element_type));
    return left;
}

TypePtr makeArrayType(TypePtr element) {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Array;
    t->element_type = std::move(element);
    return t;
}

TypePtr makeClassType(ClassSymbol* classSymbol) {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Class;
    t->class_symbol = classSymbol;
    return t;
}

TypePtr makeFunctionType(std::vector<TypePtr> params, TypePtr returnType) {
    auto t = std::make_shared<Type>();
    t->kind = TypeKind::Function;
    for (auto& param : params) if (!param) param = makeErrorType();
    t->param_types = std::move(params);
    t->return_type = returnType ? std::move(returnType) : makeErrorType();
    return t;
}

TypePtr resolveTypeAnnotation(ast::TypeAnnotation* annotation, Scope* scope,
                              diagnostics::DiagnosticReporter& reporter) {
    if (annotation == nullptr) return makeErrorType();

    if (auto* named = dynamic_cast<ast::NamedTypeAnnotation*>(annotation)) {
        if (named->name == "integer") return makeIntegerType();
        if (named->name == "boolean") return makeBooleanType();
        if (named->name == "string") return makeStringType();

        auto symbol = scope->resolve(named->name);
        auto classSymbol = std::dynamic_pointer_cast<ClassSymbol>(symbol);
        if (classSymbol == nullptr) {
            reporter.error(diagnostics::codes::SEM013,
                            "'" + named->name + "' no es un tipo valido (ni primitivo ni clase declarada).",
                            named->line, named->column);
            return makeErrorType();
        }
        return makeClassType(classSymbol.get());
    }

    if (auto* arr = dynamic_cast<ast::ArrayTypeAnnotation*>(annotation)) {
        return makeArrayType(resolveTypeAnnotation(arr->element.get(), scope, reporter));
    }

    return makeErrorType();
}

}  // namespace semantic
}  // namespace compiscript
