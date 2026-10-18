// Parte 2: funciones, llamadas, return y closures.
// Ver el documento de reparto de la parte 2, seccion 2.1 y 2.2.

#include "semantic/type.h"
#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;
using semantic::FunctionSymbol;
using semantic::TypeKind;

namespace {

FunctionSymbol* calleeFunction(Expression* callee) {
    if (auto* id = dynamic_cast<IdentifierExpression*>(callee)) {
        return dynamic_cast<FunctionSymbol*>(id->symbol);
    }
    if (auto* mem = dynamic_cast<MemberAccessExpression*>(callee)) {
        return dynamic_cast<FunctionSymbol*>(mem->symbol);
    }
    return nullptr;
}

bool callHasResult(Expression* call) {
    return call->resolved_type && call->resolved_type->kind != TypeKind::Void &&
           call->resolved_type->kind != TypeKind::Error;
}

}  // namespace

void TacGenerator::genFunctionDeclaration(FunctionDeclaration* n) {
    auto* fn = dynamic_cast<FunctionSymbol*>(n->symbol);
    if (fn == nullptr) {
        todo("parte 2", "funcion '" + n->name + "' sin simbolo");
        return;
    }
    // Abre un `func` nuevo (se apila): una anidada se genera en medio de la
    // contenedora y endFunction la guarda antes de seguir con esta.
    beginFunction(fn->label, fn, fn->frame_size);
    genBlock(n->body.get());
    endFunction();
}

void TacGenerator::genReturn(ReturnStatement* n) {
    Operand value = n->value ? genExpr(n->value.get()) : Operand::none();
    // Si no, el handler del try quedaria registrado despues de salir.
    closeTryHandlers(0);
    emitReturn(value);
    release(value);
}

Operand TacGenerator::genCall(CallExpression* n) {
    FunctionSymbol* fn = calleeFunction(n->callee.get());
    if (fn == nullptr) {
        return todo("parte 2", "llamada a un valor que no es funcion");
    }

    // 1) Evaluar TODOS los argumentos primero, de izquierda a derecha.
    //    Si se emitiera un param por argumento, una llamada anidada en un
    //    argumento (f(a, g(b))) mezclaria sus param con los de f.
    std::vector<Operand> args;
    for (auto& a : n->arguments) args.push_back(genExpr(a.get()));

    // Metodo: el receptor (`this`) se evalua despues de los argumentos,
    // igual que el orden de evaluacion de izquierda a derecha del sufijo.
    Operand thisObj = Operand::none();
    auto* member = dynamic_cast<MemberAccessExpression*>(n->callee.get());
    if (fn->is_method && member != nullptr) {
        thisObj = genExpr(member->object.get());
    } else if (fn->is_method) {
        // Metodo llamado sin receptor dentro de su clase: el receptor es
        // el 'this' actual (parametro 0, fp + 8).
        thisObj = emitLoad(Operand::makeFramePtr(), Operand::makeInt(8));
    }

    // 2) param de derecha a izquierda.
    for (auto it = args.rbegin(); it != args.rend(); ++it) emitParam(*it);
    for (auto& a : args) release(a);

    // 3) Metodo: param this al final (es el parametro 0 en el frame).
    //    Si alguna subclase lo redefine, la direccion sale de la vtable del
    //    objeto: t = this[0] (vtable), t = t[4*slot] (metodo).
    int argc = static_cast<int>(n->arguments.size());
    Operand target = Operand::none();
    if (fn->is_method) {
        emitParam(thisObj);
        if (fn->overridden && fn->vtable_slot >= 0) {
            release(thisObj);
            Operand vtable = emitLoad(thisObj, Operand::makeInt(0));
            release(vtable);
            target = emitLoad(vtable, Operand::makeInt(4 * fn->vtable_slot));
        } else {
            release(thisObj);
        }
        argc++;
    }

    // Funcion anidada: link al frame de su contenedora directa.
    if (fn->nesting_depth > 0) {
        FunctionSymbol* currentFn = current().fn.symbol;
        int walks = (currentFn != nullptr ? currentFn->nesting_depth : 0) - fn->nesting_depth + 1;
        Operand link = Operand::makeFramePtr();
        for (int i = 0; i < walks; i++) {
            Operand next = emitLoad(link, Operand::makeInt(4));
            if (link.isTemp()) release(link);
            link = next;
        }
        emitLink(link);
        if (link.isTemp()) release(link);
    }

    // 4) Llamada. hasResult si el retorno no es void.
    if (!target.isNone()) return emitCallIndirect(target, argc, callHasResult(n));
    return emitCall(fn->label, argc, callHasResult(n));
}

Operand TacGenerator::genCapturedLoad(semantic::Symbol* symbol) {
    FunctionSymbol* owner = symbol->owner_function;
    FunctionSymbol* currentFn = current().fn.symbol;
    int levels = 0;
    if (owner != nullptr && currentFn != nullptr) {
        levels = currentFn->nesting_depth - owner->nesting_depth;
    }

    // Subir por los enlaces de acceso (fp[4]) hasta el frame dueño.
    Operand frame = Operand::makeFramePtr();
    for (int i = 0; i < levels; i++) {
        Operand next = emitLoad(frame, Operand::makeInt(4));
        if (frame.isTemp()) release(frame);
        frame = next;
    }
    Operand result = emitLoad(frame, Operand::makeInt(symbol->offset));
    if (frame.isTemp()) release(frame);
    return result;
}

void TacGenerator::genCapturedStore(semantic::Symbol* symbol, const Operand& value) {
    // NO liberar `value`: storeVar lo libera despues.
    FunctionSymbol* owner = symbol->owner_function;
    FunctionSymbol* currentFn = current().fn.symbol;
    int levels = 0;
    if (owner != nullptr && currentFn != nullptr) {
        levels = currentFn->nesting_depth - owner->nesting_depth;
    }

    Operand frame = Operand::makeFramePtr();
    for (int i = 0; i < levels; i++) {
        Operand next = emitLoad(frame, Operand::makeInt(4));
        if (frame.isTemp()) release(frame);
        frame = next;
    }
    emitStore(frame, Operand::makeInt(symbol->offset), value);
    if (frame.isTemp()) release(frame);
}

}  // namespace codegen
}  // namespace compiscript
