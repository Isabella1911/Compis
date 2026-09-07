#ifndef COMPISCRIPT_RUNTIME_TYPE_DESCRIPTOR_H
#define COMPISCRIPT_RUNTIME_TYPE_DESCRIPTOR_H

// Descriptores de layout para el GC. Un TypeDescriptor dice, para cada
// slot de un objeto o arreglo, si el mark debe seguirlo como puntero al
// heap o tratarlo como dato plano (integer/boolean/string).
//
// Se construyen a partir de ClassSymbol / Type ya resueltos en la etapa
// semantica; no recalculan tipos.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace compiscript {
namespace runtime {

enum class SlotKind : uint8_t {
    Primitive,  // integer, boolean, string (hoy: no se sigue en mark)
    Pointer,    // referencia a otro bloque del heap (clase o arreglo)
};

struct FieldSlot {
    std::string name;
    SlotKind kind = SlotKind::Primitive;
    // Offset en palabras (indice) dentro del payload del objeto.
    std::size_t word_index = 0;
};

// Descriptor de instancia de clase (incluye campos heredados, en orden
// base -> derivada, para que el layout sea estable).
struct ClassDescriptor {
    std::string class_name;
    std::vector<FieldSlot> fields;

    std::size_t payloadWords() const { return fields.size(); }
};

// Descriptor de arreglo homogeneo: todos los elementos son Primitive o
// todos son Pointer, segun el tipo de elemento.
struct ArrayDescriptor {
    SlotKind element_kind = SlotKind::Primitive;
};

enum class BlockKind : uint8_t { ClassInstance, Array };

}  // namespace runtime
}  // namespace compiscript

#endif  // COMPISCRIPT_RUNTIME_TYPE_DESCRIPTOR_H
