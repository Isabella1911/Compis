#ifndef COMPISCRIPT_RUNTIME_HEAP_H
#define COMPISCRIPT_RUNTIME_HEAP_H

// Heap del runtime: unico lugar que reserva bloques dinamicos. El codigo
// generado (futuro) y las pruebas deben usar Heap::alloc*, nunca malloc
// directo, para que el GC conozca todos los objetos vivos.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "runtime/type_descriptor.h"

namespace compiscript {
namespace runtime {

struct ObjectHeader {
    BlockKind kind = BlockKind::ClassInstance;
    bool marked = false;
    // Para ClassInstance: descriptor de la clase. Para Array: nullptr
    // (el kind de elemento vive en ArrayMeta).
    const ClassDescriptor* class_desc = nullptr;
    std::size_t payload_words = 0;
};

// Metadatos extras solo para arreglos (longitud + kind de elemento).
struct ArrayMeta {
    const ArrayDescriptor* array_desc = nullptr;
    std::size_t length = 0;
};

// Bloque opaco del heap. El payload son palabras del tamano de un
// puntero: un primitivo cabe en una palabra; un puntero a otro Object
// tambien.
struct HeapObject {
    ObjectHeader header;
    ArrayMeta array;  // valido solo si header.kind == Array
    // payload[i] interpreta como valor plano o como HeapObject*
    // segun el descriptor.
    std::vector<std::uintptr_t> payload;
};

class Heap {
public:
    HeapObject* allocClass(const ClassDescriptor* desc);
    HeapObject* allocArray(const ArrayDescriptor* desc, std::size_t length);

    // Acceso tipado al payload (palabra i).
    static void setWord(HeapObject* obj, std::size_t index, std::uintptr_t value);
    static std::uintptr_t getWord(const HeapObject* obj, std::size_t index);
    static void setPointer(HeapObject* obj, std::size_t index, HeapObject* ptr);
    static HeapObject* getPointer(const HeapObject* obj, std::size_t index);

    const std::vector<std::unique_ptr<HeapObject>>& objects() const { return objects_; }
    std::size_t liveCount() const { return objects_.size(); }
    std::size_t totalBytesApprox() const;

    // Usado por el GC tras el sweep: libera los no marcados.
    void sweepUnmarked();

private:
    std::vector<std::unique_ptr<HeapObject>> objects_;
};

}  // namespace runtime
}  // namespace compiscript

#endif  // COMPISCRIPT_RUNTIME_HEAP_H
