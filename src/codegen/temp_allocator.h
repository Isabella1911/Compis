#ifndef COMPISCRIPT_CODEGEN_TEMP_ALLOCATOR_H
#define COMPISCRIPT_CODEGEN_TEMP_ALLOCATOR_H

// Asignacion y reciclaje de temporales (t0, t1, ...).
//
// Algoritmo: un temporal guarda un valor intermedio que se consume
// exactamente una vez (como operando de la instruccion siguiente). En
// cuanto se consume, quien lo uso lo libera con release() y vuelve a la
// lista de libres. acquire() siempre entrega el libre de numero mas bajo,
// asi que una expresion usa tantos temporales como valores intermedios
// esten vivos A LA VEZ, no uno por operacion: `a*b + c*d` usa t0 y t1, no
// t0..t2. Eso hace que en la fase 3 casi todo quepa en $t0-$t9.
//
// Un TempAllocator por funcion: cada `func` empieza de nuevo en t0.

#include <set>

#include "ir/tac.h"

namespace compiscript {
namespace codegen {

class TempAllocator {
public:
    ir::Operand acquire();

    // Devuelve el temporal a la lista de libres. Si `operand` no es un
    // temporal (variable, constante...), no hace nada: asi el llamador puede
    // liberar cualquier operando que haya consumido sin preguntar que es.
    void release(const ir::Operand& operand);

    // Cuantos temporales distintos se usaron como maximo al mismo tiempo.
    int maxUsed() const { return next_; }

    // Cuantos estan vivos ahora (adquiridos y no liberados). Sirve para
    // detectar fugas: al terminar cada sentencia deberia ser 0.
    int live() const { return next_ - static_cast<int>(free_.size()); }

private:
    std::set<int> free_;  // ordenado: begin() es el libre mas bajo
    int next_ = 0;        // el siguiente indice nunca usado
};

}  // namespace codegen
}  // namespace compiscript

#endif  // COMPISCRIPT_CODEGEN_TEMP_ALLOCATOR_H
