#ifndef COMPISCRIPT_RUNTIME_GC_H
#define COMPISCRIPT_RUNTIME_GC_H

// Mark-and-sweep clasico. Maneja ciclos (a diferencia del conteo de
// referencias) y usa una pila explicita en mark para no depender del
// stack de C++ del propio colector.
//
// Punto seguro tipico (curso): antes de cada alloc, si el heap supera
// `threshold_objects`, se corre collect().

#include <cstddef>
#include <vector>

#include "runtime/heap.h"

namespace compiscript {
namespace runtime {

class GarbageCollector {
public:
    explicit GarbageCollector(Heap& heap, std::size_t threshold_objects = 64);

    // Raices del mark: globales, locales vivas y capturas de closures
    // activas. El backend futuro las registrara; las pruebas las fijan a mano.
    void clearRoots();
    void addRoot(HeapObject* obj);

    // Mark desde raices + sweep. Devuelve cuantos objetos se liberaron.
    std::size_t collect();

    // Reserva un objeto de clase; puede disparar collect() si el umbral
    // se supera. Equivalente conceptual a gc_alloc(size, descriptor).
    HeapObject* allocClass(const ClassDescriptor* desc);
    HeapObject* allocArray(const ArrayDescriptor* desc, std::size_t length);

    std::size_t threshold() const { return threshold_objects_; }
    void setThreshold(std::size_t n) { threshold_objects_ = n; }

private:
    void markFromRoots();
    void markObject(HeapObject* obj);

    Heap& heap_;
    std::vector<HeapObject*> roots_;
    std::size_t threshold_objects_;
};

}  // namespace runtime
}  // namespace compiscript

#endif  // COMPISCRIPT_RUNTIME_GC_H
