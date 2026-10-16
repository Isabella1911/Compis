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

bool isRelational(const std::string& op) {
    return op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=";
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

    if (op == "&&" || op == "||") {
        // Como valor (p. ej. `let ok = a && b;`): se evalua con saltos para
        // respetar el cortocircuito y se materializa en 1 o 0.
        std::string falseLabel = newLabel();
        std::string endLabel = newLabel();
        genCond(n, "", falseLabel);
        Operand result = newTemp();
        emitCopy(result, Operand::makeInt(1));
        emitGoto(endLabel);
        emitLabel(falseLabel);
        emitCopy(result, Operand::makeInt(0));
        emitLabel(endLabel);
        return result;
    }

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

Operand TacGenerator::genTernary(TernaryExpression* n) {
    std::string elseLabel = newLabel();
    std::string endLabel = newLabel();

    genCond(n->condition.get(), "", elseLabel);
    Operand thenValue = genExpr(n->then_expr.get());
    // Las dos ramas escriben el MISMO temporal; queda reservado mientras se
    // evalua la rama else.
    Operand result = newTemp();
    assignTo(result, thenValue);
    emitGoto(endLabel);

    emitLabel(elseLabel);
    assignTo(result, genExpr(n->else_expr.get()));
    emitLabel(endLabel);
    return result;
}

// Codigo con saltos (apuntes: "codigo de corto circuito"). Cada caso emite
// solo los saltos necesarios segun cual de las dos salidas cae de largo:
//   ambas etiquetas -> if c goto T; goto F
//   solo T          -> if c goto T            (falso sigue de largo)
//   solo F          -> ifFalse c goto F       (verdadero sigue de largo)
void TacGenerator::genCond(Expression* expr, const std::string& trueLabel,
                           const std::string& falseLabel) {
    if (auto* n = dynamic_cast<BinaryExpression*>(expr)) {
        if (n->op == "&&") {
            // Si la izquierda es falsa, ya no se evalua la derecha.
            std::string leftFalse = falseLabel.empty() ? newLabel() : falseLabel;
            genCond(n->left.get(), "", leftFalse);
            genCond(n->right.get(), trueLabel, falseLabel);
            if (falseLabel.empty()) emitLabel(leftFalse);
            return;
        }
        if (n->op == "||") {
            // Si la izquierda es verdadera, ya no se evalua la derecha.
            std::string leftTrue = trueLabel.empty() ? newLabel() : trueLabel;
            genCond(n->left.get(), leftTrue, "");
            genCond(n->right.get(), trueLabel, falseLabel);
            if (trueLabel.empty()) emitLabel(leftTrue);
            return;
        }
        // Comparacion entre enteros (o punteros): un solo salto relacional,
        // sin materializar el booleano. Entre strings compara contenido y
        // pasa por __str_eq (caso general de abajo).
        bool stringOperands = isStringType(n->left.get()) && isStringType(n->right.get());
        if (isRelational(n->op) && !stringOperands) {
            Operand left = genExpr(n->left.get());
            Operand right = genExpr(n->right.get());
            release(left);
            release(right);
            if (!trueLabel.empty()) {
                emitIfRel(left, n->op, right, trueLabel);
                if (!falseLabel.empty()) emitGoto(falseLabel);
            } else if (!falseLabel.empty()) {
                emitIfFalseRel(left, n->op, right, falseLabel);
            }
            return;
        }
    }
    if (auto* n = dynamic_cast<UnaryExpression*>(expr)) {
        if (n->op == "!") {
            genCond(n->operand.get(), falseLabel, trueLabel);  // se intercambian las salidas
            return;
        }
    }
    if (auto* n = dynamic_cast<LiteralExpression*>(expr)) {
        if (n->kind == LiteralKind::Boolean) {
            const std::string& target = n->value == "true" ? trueLabel : falseLabel;
            if (!target.empty()) emitGoto(target);
            return;
        }
    }

    // Caso general: cualquier valor booleano (variable, llamada, ...).
    Operand value = genExpr(expr);
    release(value);
    if (!trueLabel.empty()) {
        emitIf(value, trueLabel);
        if (!falseLabel.empty()) emitGoto(falseLabel);
    } else if (!falseLabel.empty()) {
        emitIfFalse(value, falseLabel);
    }
}

}  // namespace codegen
}  // namespace compiscript
