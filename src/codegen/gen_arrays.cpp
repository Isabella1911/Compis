// Parte 2: listas, acceso por indice y foreach.
// Ver el documento de reparto de la parte 2, seccion 2.4.
//
// Layout de una lista en el heap (todo en palabras de 4 bytes):
//
//     [0]  longitud N
//     [4]  elemento 0
//     [8]  elemento 1
//     ...
//     [4 + 4*(N-1)]  elemento N-1

#include <vector>

#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;

namespace {

constexpr int kWordSize = 4;
constexpr int kLengthOffset = 0;
constexpr int kDataOffset = 4;

}  // namespace

Operand TacGenerator::genArrayLiteral(ArrayLiteral* n) {
    // Elementos primero (izquierda a derecha), despues alloc y stores.
    std::vector<Operand> elems;
    for (auto& e : n->elements) elems.push_back(genExpr(e.get()));

    int nElems = static_cast<int>(elems.size());
    Operand arr = emitAlloc(Operand::makeInt(kDataOffset + kWordSize * nElems));
    emitStore(arr, Operand::makeInt(kLengthOffset), Operand::makeInt(nElems));
    for (int i = 0; i < nElems; i++) {
        emitStore(arr, Operand::makeInt(kDataOffset + kWordSize * i), elems[static_cast<size_t>(i)]);
        release(elems[static_cast<size_t>(i)]);
    }
    return arr;
}

Operand TacGenerator::genArrayAccess(ArrayAccessExpression* n) {
    Operand arr = genExpr(n->array.get());
    Operand index = genExpr(n->index.get());

    // if index < 0 || index >= length -> __raise
    Operand length = emitLoad(arr, Operand::makeInt(kLengthOffset));
    std::string okLabel = newLabel();
    std::string errLabel = newLabel();
    emitIfRel(index, "<", Operand::makeInt(0), errLabel);
    emitIfRel(index, "<", length, okLabel);
    emitLabel(errLabel);
    emitParam(internString("indice fuera de rango"));
    emitCall("__raise", 1, false);
    emitLabel(okLabel);
    release(length);

    // offset = 4 + index * 4
    release(index);
    Operand offset = newTemp();
    emitBinary(offset, "*", index, Operand::makeInt(kWordSize));
    emitBinary(offset, "+", offset, Operand::makeInt(kDataOffset));
    Operand result = emitLoad(arr, offset);
    release(arr);
    release(offset);
    return result;
}

Operand TacGenerator::genIndexAssignment(ArrayAccessExpression* target, Expression* value) {
    Operand arr = genExpr(target->array.get());
    Operand index = genExpr(target->index.get());
    Operand val = genExpr(value);

    Operand length = emitLoad(arr, Operand::makeInt(kLengthOffset));
    std::string okLabel = newLabel();
    std::string errLabel = newLabel();
    emitIfRel(index, "<", Operand::makeInt(0), errLabel);
    emitIfRel(index, "<", length, okLabel);
    emitLabel(errLabel);
    emitParam(internString("indice fuera de rango"));
    emitCall("__raise", 1, false);
    emitLabel(okLabel);
    release(length);

    release(index);
    Operand offset = newTemp();
    emitBinary(offset, "*", index, Operand::makeInt(kWordSize));
    emitBinary(offset, "+", offset, Operand::makeInt(kDataOffset));
    emitStore(arr, offset, val);
    release(arr);
    release(offset);
    // El valor de `a[i] = v` es v.
    return val;
}

void TacGenerator::genForeach(ForeachStatement* n) {
    semantic::Symbol* var = n->scope->resolveLocal(n->var_name).get();
    Operand arr = genExpr(n->iterable.get());
    Operand length = emitLoad(arr, Operand::makeInt(kLengthOffset));
    // Indice oculto: temporal que vive todo el bucle.
    Operand index = newTemp();
    emitCopy(index, Operand::makeInt(0));

    std::string beginLabel = newLabel();
    std::string updateLabel = newLabel();  // destino de continue
    std::string endLabel = newLabel();

    emitLabel(beginLabel);
    emitIfFalseRel(index, "<", length, endLabel);

    // var = arr[4 + index * 4]
    Operand offset = newTemp();
    emitBinary(offset, "*", index, Operand::makeInt(kWordSize));
    emitBinary(offset, "+", offset, Operand::makeInt(kDataOffset));
    Operand elem = emitLoad(arr, offset);
    release(offset);
    storeVar(var, elem);

    pushLoop(endLabel, updateLabel);
    genBlock(n->body.get());
    popLoop();

    emitLabel(updateLabel);
    emitBinary(index, "+", index, Operand::makeInt(1));
    emitGoto(beginLabel);
    emitLabel(endLabel);

    release(index);
    release(length);
    release(arr);
}

}  // namespace codegen
}  // namespace compiscript
