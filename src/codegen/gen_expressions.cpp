// Parte 1: expresiones y condiciones con saltos.

#include <string>

#include "semantic/type.h"
#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;
using semantic::TypeKind;

namespace {

bool isStringType(const Expression* e) {
    return e->resolved_type && e->resolved_type->kind == TypeKind::String;
}

// El lexer deja el literal con sus comillas: "hola" -> hola. Las
// secuencias de escape (\n, \") se dejan tal cual; son las mismas que
// entiende .asciiz en MIPS.
std::string stripQuotes(const std::string& text) {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

}  // namespace

Operand TacGenerator::genLiteral(LiteralExpression* n) {
    switch (n->kind) {
        case LiteralKind::Integer: return Operand::makeInt(static_cast<int>(std::stol(n->value)));
        case LiteralKind::String: return internString(stripQuotes(n->value));
        case LiteralKind::Boolean: return Operand::makeInt(n->value == "true" ? 1 : 0);
        case LiteralKind::Null: return Operand::makeInt(0);
    }
    return Operand::makeInt(0);
}

Operand TacGenerator::genIdentifier(IdentifierExpression* n) {
    if (dynamic_cast<semantic::FunctionSymbol*>(n->symbol) != nullptr) {
        // Una funcion usada como valor (no llamada): es de la parte 2.
        return todo("parte 2", "funcion '" + n->name + "' usada como valor");
    }
    return loadVar(n->symbol);
}

Operand TacGenerator::genBinary(BinaryExpression* n) {
    const std::string& op = n->op;

    if (op == "&&" || op == "||") return todo("parte 1", "operador logico " + op);

    // Orden de evaluacion: izquierda y despues derecha, como en TypeScript.
    Operand left = genExpr(n->left.get());
    Operand right = genExpr(n->right.get());

    // Los strings son punteros: concatenar o comparar su contenido no es
    // una operacion de una instruccion, se delega al runtime.
    bool stringOperands = isStringType(n->left.get()) && isStringType(n->right.get());
    if (stringOperands && (op == "+" || op == "==" || op == "!=")) {
        emitParam(right);  // los param van de derecha a izquierda
        emitParam(left);
        release(left);
        release(right);
        Operand result = emitCall(op == "+" ? "__concat" : "__str_eq", 2, true);
        if (op == "!=") emitUnary(result, "!", result);
        return result;
    }

    // Se liberan los operandos ANTES de pedir el destino: asi el resultado
    // reusa el temporal de un operando (t0 = t0 + t1) y una expresion usa
    // tantos temporales como valores intermedios esten vivos a la vez.
    release(left);
    release(right);
    Operand result = newTemp();
    emitBinary(result, op, left, right);
    return result;
}

Operand TacGenerator::genUnary(UnaryExpression* n) {
    Operand operand = genExpr(n->operand.get());
    release(operand);
    Operand result = newTemp();
    emitUnary(result, n->op, operand);
    return result;
}

Operand TacGenerator::genAssignment(AssignmentExpression* n) {
    if (auto* target = dynamic_cast<ArrayAccessExpression*>(n->target.get())) {
        return genIndexAssignment(target, n->value.get());
    }
    if (auto* target = dynamic_cast<MemberAccessExpression*>(n->target.get())) {
        return genPropertyAssignment(target->object.get(), target->symbol, n->value.get());
    }

    auto* target = dynamic_cast<IdentifierExpression*>(n->target.get());
    if (target == nullptr || target->symbol == nullptr) {
        return todo("parte 1", "destino de asignacion no soportado");
    }
    storeVar(target->symbol, genExpr(n->value.get()));
    // El valor de `x = v` es x: permite encadenar `y = x = 3`.
    return loadVar(target->symbol);
}

Operand TacGenerator::genTernary(TernaryExpression* /*n*/) {
    return todo("parte 1", "ternario");
}

void TacGenerator::genCond(Expression* /*expr*/, const std::string& /*trueLabel*/,
                           const std::string& /*falseLabel*/) {
    todo("parte 1", "condicion");
}

}  // namespace codegen
}  // namespace compiscript
