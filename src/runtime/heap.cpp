#include "runtime/heap.h"

#include <stdexcept>

namespace compiscript {
namespace runtime {

HeapObject* Heap::allocClass(const ClassDescriptor* desc) {
    if (desc == nullptr) {
        throw std::invalid_argument("allocClass: descriptor nulo");
    }
    auto obj = std::make_unique<HeapObject>();
    obj->header.kind = BlockKind::ClassInstance;
    obj->header.marked = false;
    obj->header.class_desc = desc;
    obj->header.payload_words = desc->payloadWords();
    obj->payload.assign(obj->header.payload_words, 0);
    HeapObject* raw = obj.get();
    objects_.push_back(std::move(obj));
    return raw;
}

HeapObject* Heap::allocArray(const ArrayDescriptor* desc, std::size_t length) {
    if (desc == nullptr) {
        throw std::invalid_argument("allocArray: descriptor nulo");
    }
    auto obj = std::make_unique<HeapObject>();
    obj->header.kind = BlockKind::Array;
    obj->header.marked = false;
    obj->header.class_desc = nullptr;
    obj->header.payload_words = length;
    obj->array.array_desc = desc;
    obj->array.length = length;
    obj->payload.assign(length, 0);
    HeapObject* raw = obj.get();
    objects_.push_back(std::move(obj));
    return raw;
}

void Heap::setWord(HeapObject* obj, std::size_t index, std::uintptr_t value) {
    if (obj == nullptr || index >= obj->payload.size()) {
        throw std::out_of_range("Heap::setWord");
    }
    obj->payload[index] = value;
}

std::uintptr_t Heap::getWord(const HeapObject* obj, std::size_t index) {
    if (obj == nullptr || index >= obj->payload.size()) {
        throw std::out_of_range("Heap::getWord");
    }
    return obj->payload[index];
}

void Heap::setPointer(HeapObject* obj, std::size_t index, HeapObject* ptr) {
    setWord(obj, index, reinterpret_cast<std::uintptr_t>(ptr));
}

HeapObject* Heap::getPointer(const HeapObject* obj, std::size_t index) {
    return reinterpret_cast<HeapObject*>(getWord(obj, index));
}

std::size_t Heap::totalBytesApprox() const {
    std::size_t n = 0;
    for (const auto& o : objects_) {
        n += sizeof(HeapObject) + o->payload.size() * sizeof(std::uintptr_t);
    }
    return n;
}

void Heap::sweepUnmarked() {
    std::vector<std::unique_ptr<HeapObject>> survivors;
    survivors.reserve(objects_.size());
    for (auto& obj : objects_) {
        if (obj->header.marked) {
            obj->header.marked = false;  // listo para el proximo ciclo
            survivors.push_back(std::move(obj));
        }
    }
    objects_.swap(survivors);
}

}  // namespace runtime
}  // namespace compiscript
