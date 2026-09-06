#include "type_checker.h"

#include "diagnostics/codes.h"
#include "symbol.h"

namespace compiscript {
namespace semantic {

using namespace compiscript::ast;

TypeChecker::TypeChecker(diagnostics::DiagnosticReporter& reporter) : reporter_(reporter) {}

TypePtr TypeChecker::resolveDeclaredType(const ast::TypeAnnotationPtr& annotation, Scope* scope) {
    if (!annotation) return nullptr;
    return resolveTypeAnnotation(annotation.get(), scope, reporter_);
}

bool TypeChecker::checkCompatible(const TypePtr& expected, const TypePtr& actual, const char* code,
                                   const std::string& message, int line, int column) {
    if (!expected || !actual) return true;  // uno de los dos ya es un tipo desconocido/no resuelto
    if (expected->equals(*actual)) return true;
    reporter_.error(code, message, line, column);
    return false;
}

Scope* TypeChecker::findEnclosingFunctionScope(Scope* scope) const {
    for (Scope* s = scope; s != nullptr; s = s->parent()) {
        if (s->kind() == ScopeKind::Function) return s;
    }
    return nullptr;
}

Scope* TypeChecker::findEnclosingClassScope(Scope* scope) const {
    for (Scope* s = scope; s != nullptr; s = s->parent()) {
        if (s->kind() == ScopeKind::Class) return s;
    }
    return nullptr;
}

Symbol* TypeChecker::lookupMember(ClassSymbol* cls, const std::string& name) const {
    for (ClassSymbol* current = cls; current != nullptr; current = current->base_class) {
        if (current->class_scope == nullptr) continue;
        if (auto sym = current->class_scope->resolveLocal(name)) return sym.get();
    }
    return nullptr;
}

Symbol* TypeChecker::resolveMemberAccess(Expression* objectExpr, const std::string& memberName,
                                          Scope* scope, int line, int column) {
    TypePtr objectType = checkExpression(objectExpr, scope);
    if (!objectType || objectType->kind == TypeKind::Error) {
        // El objeto ya arrastra un problema anterior (p. ej. una variable
        // no declarada, ya reportada por NameResolver): no reportar de
        // nuevo sobre el acceso a miembro.
        return nullptr;
    }
    if (objectType->kind != TypeKind::Class) {
        reporter_.error(diagnostics::codes::SEM010,
                         "no se puede acceder a '." + memberName +
                             "': el valor no es un objeto de una clase.",
                         line, column);
        return nullptr;
    }
    Symbol* member = lookupMember(objectType->class_symbol, memberName);
    if (member == nullptr) {
        reporter_.error(diagnostics::codes::SEM010,
                         "'" + memberName + "' no existe en la clase '" +
                             objectType->class_symbol->name + "' ni en sus clases base.",
                         line, column);
        return nullptr;
    }
    return member;
}

void TypeChecker::checkArguments(const std::vector<TypePtr>& paramTypes,
                                  const std::vector<TypePtr>& argTypes, int line, int column) {
    if (paramTypes.size() != argTypes.size()) {
        reporter_.error(diagnostics::codes::SEM008,
                         "se esperaban " + std::to_string(paramTypes.size()) +
                             " argumento(s), se recibieron " + std::to_string(argTypes.size()) +
                             ".",
                         line, column);
        return;
    }
    for (size_t i = 0; i < paramTypes.size(); i++) {
        checkCompatible(paramTypes[i], argTypes[i], diagnostics::codes::SEM008,
                         "el argumento " + std::to_string(i + 1) +
                             " no coincide con el tipo esperado.",
                         line, column);
    }
}

void TypeChecker::run(Program& program) {
    for (auto& stmt : program.statements) {
        checkStatement(stmt.get());
    }
}

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

void TypeChecker::checkStatement(Statement* stmt) {
    Scope* scope = stmt->scope;

    if (auto* n = dynamic_cast<VariableDeclaration*>(stmt)) {
        TypePtr initType;
        if (n->initializer) initType = checkExpression(n->initializer.get(), scope);
        TypePtr declaredType = resolveDeclaredType(n->declared_type, scope);

        TypePtr finalType;
        if (declaredType && initType) {
            checkCompatible(declaredType, initType, diagnostics::codes::SEM003,
                             "el valor inicial no coincide con el tipo declarado de '" + n->name +
                                 "'.",
                             n->line, n->column);
            finalType = declaredType;
        } else if (declaredType) {
            finalType = declaredType;
        } else if (initType) {
            finalType = initType;  // inferido del inicializador
        } else {
            finalType = makeErrorType();  // 'let x;' sin tipo ni inicializador
        }
        n->resolved_type = finalType;
        if (n->symbol) n->symbol->resolved_type = finalType;
        return;
    }
    if (auto* n = dynamic_cast<ConstantDeclaration*>(stmt)) {
        TypePtr initType = checkExpression(n->initializer.get(), scope);
        TypePtr declaredType = resolveDeclaredType(n->declared_type, scope);
        TypePtr finalType = declaredType ? declaredType : initType;
        if (declaredType) {
            checkCompatible(declaredType, initType, diagnostics::codes::SEM003,
                             "el valor inicial no coincide con el tipo declarado de '" + n->name +
                                 "'.",
                             n->line, n->column);
        }
        n->resolved_type = finalType;
        if (n->symbol) n->symbol->resolved_type = finalType;
        return;
    }
    if (auto* n = dynamic_cast<AssignmentStatement*>(stmt)) {
        TypePtr valueType = checkExpression(n->value.get(), scope);
        TypePtr targetType = n->symbol ? n->symbol->resolved_type : nullptr;
        checkCompatible(targetType, valueType, diagnostics::codes::SEM003,
                         "el valor asignado no coincide con el tipo de '" + n->target_name + "'.",
                         n->line, n->column);
        n->resolved_type = valueType;
        return;
    }
    if (auto* n = dynamic_cast<PropertyAssignment*>(stmt)) {
        Symbol* member = resolveMemberAccess(n->object.get(), n->member_name, scope, n->line,
                                             n->column);
        TypePtr valueType = checkExpression(n->value.get(), scope);
        if (member != nullptr) {
            checkCompatible(member->resolved_type, valueType, diagnostics::codes::SEM003,
                             "el valor asignado no coincide con el tipo de '." + n->member_name +
                                 "'.",
                             n->line, n->column);
        }
        n->symbol = member;
        n->resolved_type = valueType;
        return;
    }
    if (auto* n = dynamic_cast<ExpressionStatement*>(stmt)) {
        checkExpression(n->expression.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<PrintStatement*>(stmt)) {
        checkExpression(n->expression.get(), scope);
        return;
    }
    if (auto* n = dynamic_cast<Block*>(stmt)) {
        for (auto& s : n->statements) checkStatement(s.get());
        return;
    }
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) {
        TypePtr condType = checkExpression(n->condition.get(), scope);
        checkCompatible(makeBooleanType(), condType, diagnostics::codes::SEM005,
                         "la condicion del 'if' debe ser boolean.", n->condition->line,
                         n->condition->column);
        checkStatement(n->then_block.get());
        if (n->else_block) checkStatement(n->else_block.get());
        return;
    }
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) {
        TypePtr condType = checkExpression(n->condition.get(), scope);
        checkCompatible(makeBooleanType(), condType, diagnostics::codes::SEM005,
                         "la condicion del 'while' debe ser boolean.", n->condition->line,
                         n->condition->column);
        checkStatement(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) {
        checkStatement(n->body.get());
        TypePtr condType = checkExpression(n->condition.get(), scope);
        checkCompatible(makeBooleanType(), condType, diagnostics::codes::SEM005,
                         "la condicion del 'do-while' debe ser boolean.", n->condition->line,
                         n->condition->column);
        return;
    }
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) {
        if (n->init) checkStatement(n->init.get());
        if (n->condition) {
            TypePtr condType = checkExpression(n->condition.get(), scope);
            checkCompatible(makeBooleanType(), condType, diagnostics::codes::SEM005,
                             "la condicion del 'for' debe ser boolean.", n->condition->line,
                             n->condition->column);
        }
        if (n->update) checkExpression(n->update.get(), scope);
        checkStatement(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) {
        TypePtr iterableType = checkExpression(n->iterable.get(), scope);
        TypePtr elementType = (iterableType && iterableType->kind == TypeKind::Array)
                                  ? iterableType->element_type
                                  : makeErrorType();
        // 'foreach (x in ...)' nunca anota tipo en la gramatica: se infiere
        // del elemento del arreglo. El scope del loop (n->scope) es donde
        // DeclarationCollector declaro 'x'.
        if (auto sym = n->scope->resolveLocal(n->var_name)) sym->resolved_type = elementType;
        checkStatement(n->body.get());
        return;
    }
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        TypePtr subjectType = checkExpression(n->subject.get(), scope);
        for (auto& c : n->cases) {
            TypePtr caseType = checkExpression(c->expression.get(), scope);
            checkCompatible(subjectType, caseType, diagnostics::codes::SEM004,
                             "el valor de este 'case' no es compatible con el tipo del switch.",
                             c->expression->line, c->expression->column);
            for (auto& s : c->statements) checkStatement(s.get());
        }
        for (auto& s : n->default_statements) checkStatement(s.get());
        return;
    }
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) {
        checkStatement(n->try_block.get());
        checkStatement(n->catch_block.get());
        return;
    }
    if (auto* n = dynamic_cast<ReturnStatement*>(stmt)) {
        TypePtr valueType = n->value ? checkExpression(n->value.get(), scope) : makeVoidType();
        n->resolved_type = valueType;

        Scope* funcScope = findEnclosingFunctionScope(scope);
        if (funcScope == nullptr) {
            // Fuera de una funcion: lo reporta ControlFlowChecker (SEM007),
            // no este pass -- aca no hay tipo esperado contra el cual
            // comparar.
            return;
        }
        auto* fn = dynamic_cast<FunctionSymbol*>(funcScope->owner);
        if (fn == nullptr) return;
        TypePtr expected = fn->return_type ? resolveDeclaredType(fn->return_type, scope)
                                            : makeVoidType();
        checkCompatible(expected, valueType, diagnostics::codes::SEM009,
                         "el valor de 'return' no coincide con el tipo de retorno declarado.",
                         n->line, n->column);
        return;
    }
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) {
        Scope* funcScope = n->body->scope;
        for (auto& param : n->params) {
            TypePtr paramType = resolveDeclaredType(param.declared_type, n->scope);
            if (auto sym = funcScope->resolveLocal(param.name)) sym->resolved_type = paramType;
        }
        if (n->symbol) {
            auto* fn = dynamic_cast<FunctionSymbol*>(n->symbol);
            if (fn) {
                std::vector<TypePtr> paramTypes;
                for (auto& param : n->params) {
                    paramTypes.push_back(resolveDeclaredType(param.declared_type, n->scope));
                }
                TypePtr returnType =
                    n->return_type ? resolveDeclaredType(n->return_type, n->scope) : makeVoidType();
                fn->resolved_type = makeFunctionType(std::move(paramTypes), returnType);
            }
        }
        for (auto& s : n->body->statements) checkStatement(s.get());
        return;
    }
    if (auto* n = dynamic_cast<ClassDeclaration*>(stmt)) {
        if (n->symbol) {
            auto* cls = dynamic_cast<ClassSymbol*>(n->symbol);
            if (cls) cls->resolved_type = makeClassType(cls);
        }
        for (auto& member : n->members) checkStatement(member.get());
        return;
    }

    // BreakStatement, ContinueStatement: no tienen expresiones ni tipo.
}

// ---------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------

TypePtr TypeChecker::checkExpression(Expression* expr, Scope* scope) {
    if (expr == nullptr) return makeErrorType();

    if (auto* n = dynamic_cast<AssignmentExpression*>(expr)) {
        TypePtr targetType = checkExpression(n->target.get(), scope);
        TypePtr valueType = checkExpression(n->value.get(), scope);
        checkCompatible(targetType, valueType, diagnostics::codes::SEM003,
                         "el valor asignado no coincide con el tipo de la variable.", n->line,
                         n->column);
        n->resolved_type = targetType ? targetType : valueType;
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<PropertyAssignExpr*>(expr)) {
        Symbol* member = resolveMemberAccess(n->object.get(), n->member_name, scope, n->line,
                                             n->column);
        TypePtr valueType = checkExpression(n->value.get(), scope);
        if (member != nullptr) {
            checkCompatible(member->resolved_type, valueType, diagnostics::codes::SEM003,
                             "el valor asignado no coincide con el tipo de '." + n->member_name +
                                 "'.",
                             n->line, n->column);
        }
        n->symbol = member;
        n->resolved_type = valueType;
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<TernaryExpression*>(expr)) {
        TypePtr condType = checkExpression(n->condition.get(), scope);
        checkCompatible(makeBooleanType(), condType, diagnostics::codes::SEM005,
                         "la condicion del operador ternario debe ser boolean.",
                         n->condition->line, n->condition->column);
        TypePtr thenType = checkExpression(n->then_expr.get(), scope);
        TypePtr elseType = checkExpression(n->else_expr.get(), scope);
        if (!checkCompatible(thenType, elseType, diagnostics::codes::SEM004,
                              "las dos ramas del operador ternario deben tener el mismo tipo.",
                              n->line, n->column)) {
            n->resolved_type = makeErrorType();
        } else {
            n->resolved_type = thenType ? thenType : elseType;
        }
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<BinaryExpression*>(expr)) {
        TypePtr leftType = checkExpression(n->left.get(), scope);
        TypePtr rightType = checkExpression(n->right.get(), scope);
        const std::string& op = n->op;

        TypePtr result;
        if (op == "&&" || op == "||") {
            bool ok = checkCompatible(makeBooleanType(), leftType, diagnostics::codes::SEM004,
                                       "el operador '" + op + "' requiere operandos boolean.",
                                       n->line, n->column) &&
                      checkCompatible(makeBooleanType(), rightType, diagnostics::codes::SEM004,
                                       "el operador '" + op + "' requiere operandos boolean.",
                                       n->line, n->column);
            result = ok ? makeBooleanType() : makeErrorType();
        } else if (op == "==" || op == "!=") {
            checkCompatible(leftType, rightType, diagnostics::codes::SEM004,
                             "no se puede comparar con '" + op + "' operandos de tipos distintos.",
                             n->line, n->column);
            result = makeBooleanType();
        } else if (op == "<" || op == "<=" || op == ">" || op == ">=") {
            bool ok = checkCompatible(makeIntegerType(), leftType, diagnostics::codes::SEM004,
                                       "el operador '" + op + "' requiere operandos integer.",
                                       n->line, n->column) &&
                      checkCompatible(makeIntegerType(), rightType, diagnostics::codes::SEM004,
                                       "el operador '" + op + "' requiere operandos integer.",
                                       n->line, n->column);
            result = makeBooleanType();
            (void)ok;  // el resultado de una comparacion siempre es boolean, ok solo controla el mensaje
        } else if (op == "+") {
            bool leftUnknown = !leftType || leftType->kind == TypeKind::Error;
            bool rightUnknown = !rightType || rightType->kind == TypeKind::Error;
            bool bothInteger = leftType && rightType && leftType->kind == TypeKind::Integer &&
                                rightType->kind == TypeKind::Integer;
            bool bothString = leftType && rightType && leftType->kind == TypeKind::String &&
                               rightType->kind == TypeKind::String;
            if (bothInteger) {
                result = makeIntegerType();
            } else if (bothString) {
                result = makeStringType();
            } else if (leftUnknown || rightUnknown) {
                // Alguno de los dos lados todavia no tiene tipo resuelto
                // (p. ej. un acceso a miembro, que esta pendiente hasta
                // resolver herencia -- ver docs/03_passes_semanticos.md).
                // No reportar en cascada por algo que ya se sabe que falta.
                result = makeErrorType();
            } else {
                reporter_.error(diagnostics::codes::SEM004,
                                 "'+' requiere dos integer o dos string (no se permite mezclar).",
                                 n->line, n->column);
                result = makeErrorType();
            }
        } else {  // '-', '*', '/', '%'
            bool ok = checkCompatible(makeIntegerType(), leftType, diagnostics::codes::SEM004,
                                       "el operador '" + op + "' requiere operandos integer.",
                                       n->line, n->column) &&
                      checkCompatible(makeIntegerType(), rightType, diagnostics::codes::SEM004,
                                       "el operador '" + op + "' requiere operandos integer.",
                                       n->line, n->column);
            result = ok ? makeIntegerType() : makeErrorType();
        }
        n->resolved_type = result;
        return result;
    }
    if (auto* n = dynamic_cast<UnaryExpression*>(expr)) {
        TypePtr operandType = checkExpression(n->operand.get(), scope);
        TypePtr result;
        if (n->op == "!") {
            bool ok = checkCompatible(makeBooleanType(), operandType, diagnostics::codes::SEM004,
                                       "el operador '!' requiere un operando boolean.", n->line,
                                       n->column);
            result = ok ? makeBooleanType() : makeErrorType();
        } else {  // '-'
            bool ok = checkCompatible(makeIntegerType(), operandType, diagnostics::codes::SEM004,
                                       "el operador '-' unario requiere un operando integer.",
                                       n->line, n->column);
            result = ok ? makeIntegerType() : makeErrorType();
        }
        n->resolved_type = result;
        return result;
    }
    if (auto* n = dynamic_cast<LiteralExpression*>(expr)) {
        switch (n->kind) {
            case LiteralKind::Integer: n->resolved_type = makeIntegerType(); break;
            case LiteralKind::String: n->resolved_type = makeStringType(); break;
            case LiteralKind::Boolean: n->resolved_type = makeBooleanType(); break;
            case LiteralKind::Null: n->resolved_type = makeNullType(); break;
        }
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<ArrayLiteral*>(expr)) {
        TypePtr elementType;
        for (auto& element : n->elements) {
            TypePtr elType = checkExpression(element.get(), scope);
            if (!elementType) {
                elementType = elType;
            } else {
                checkCompatible(elementType, elType, diagnostics::codes::SEM004,
                                 "todos los elementos de un arreglo deben ser del mismo tipo.",
                                 element->line, element->column);
            }
        }
        n->resolved_type = makeArrayType(elementType ? elementType : makeErrorType());
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<IdentifierExpression*>(expr)) {
        // SEM001 (no declarado) ya lo reporto NameResolver; si symbol es
        // null aca es justamente ese caso, no hay que reportar de nuevo.
        n->resolved_type = n->symbol ? n->symbol->resolved_type : makeErrorType();
        if (!n->resolved_type) n->resolved_type = makeErrorType();
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<ThisExpression*>(expr)) {
        Scope* classScope = findEnclosingClassScope(scope);
        if (classScope == nullptr) {
            reporter_.error(diagnostics::codes::SEM015,
                             "'this' solo puede usarse dentro de un metodo de clase.", n->line,
                             n->column);
            n->resolved_type = makeErrorType();
            return n->resolved_type;
        }
        auto* cls = dynamic_cast<ClassSymbol*>(classScope->owner);
        n->symbol = classScope->owner;
        n->resolved_type = cls ? makeClassType(cls) : makeErrorType();
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<NewExpression*>(expr)) {
        std::vector<TypePtr> argTypes;
        for (auto& arg : n->arguments) argTypes.push_back(checkExpression(arg.get(), scope));
        // n->symbol ya lo resolvio NameResolver (verifica que la clase exista).
        auto* cls = n->symbol ? dynamic_cast<ClassSymbol*>(n->symbol) : nullptr;
        if (cls != nullptr) {
            Symbol* ctor = lookupMember(cls, "constructor");
            auto* ctorFn = ctor ? dynamic_cast<FunctionSymbol*>(ctor) : nullptr;
            if (ctorFn != nullptr && ctorFn->resolved_type) {
                checkArguments(ctorFn->resolved_type->param_types, argTypes, n->line, n->column);
            } else if (ctorFn == nullptr && !n->arguments.empty()) {
                // "si existe" (PDF): sin constructor definido, no deberia
                // recibir argumentos.
                reporter_.error(diagnostics::codes::SEM008,
                                 "la clase '" + cls->name +
                                     "' no tiene 'constructor', pero se le pasaron argumentos.",
                                 n->line, n->column);
            }
        }
        n->resolved_type = cls ? makeClassType(cls) : makeErrorType();
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<CallExpression*>(expr)) {
        TypePtr calleeType = checkExpression(n->callee.get(), scope);
        std::vector<TypePtr> argTypes;
        for (auto& arg : n->arguments) argTypes.push_back(checkExpression(arg.get(), scope));
        if (calleeType && calleeType->kind == TypeKind::Function) {
            checkArguments(calleeType->param_types, argTypes, n->line, n->column);
            n->resolved_type = calleeType->return_type;
        } else {
            n->resolved_type = makeErrorType();
        }
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<ArrayAccessExpression*>(expr)) {
        TypePtr arrayType = checkExpression(n->array.get(), scope);
        TypePtr indexType = checkExpression(n->index.get(), scope);
        checkCompatible(makeIntegerType(), indexType, diagnostics::codes::SEM004,
                         "el indice de un arreglo debe ser integer.", n->index->line,
                         n->index->column);
        if (arrayType && arrayType->kind == TypeKind::Array) {
            n->resolved_type = arrayType->element_type;
        } else if (!arrayType || arrayType->kind == TypeKind::Error) {
            // El tipo de 'array' todavia no se pudo resolver (p. ej. viene
            // de un acceso a miembro pendiente) -- no reportar en cascada.
            n->resolved_type = makeErrorType();
        } else {
            reporter_.error(diagnostics::codes::SEM004,
                             "solo se puede indexar con '[]' un valor de tipo arreglo.", n->line,
                             n->column);
            n->resolved_type = makeErrorType();
        }
        return n->resolved_type;
    }
    if (auto* n = dynamic_cast<MemberAccessExpression*>(expr)) {
        Symbol* member =
            resolveMemberAccess(n->object.get(), n->member_name, scope, n->line, n->column);
        n->symbol = member;
        n->resolved_type = (member != nullptr && member->resolved_type) ? member->resolved_type
                                                                          : makeErrorType();
        return n->resolved_type;
    }

    return makeErrorType();
}

}  // namespace semantic
}  // namespace compiscript
