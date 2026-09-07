#include "runtime/gc.h"

#include <stack>

namespace compiscript {
namespace runtime {

GarbageCollector::GarbageCollector(Heap& heap, std::size_t threshold_objects)
    : heap_(heap), threshold_objects_(threshold_objects) {}

void GarbageCollector::clearRoots() { roots_.clear(); }

void GarbageCollector::addRoot(HeapObject* obj) {
    if (obj != nullptr) {
        roots_.push_back(obj);
    }
}

void GarbageCollector::markObject(HeapObject* obj) {
    if (obj == nullptr || obj->header.marked) {
        return;
    }
    std::stack<HeapObject*> work;
    work.push(obj);
    while (!work.empty()) {
        HeapObject* current = work.top();
        work.pop();
        if (current == nullptr || current->header.marked) {
            continue;
        }
        current->header.marked = true;

        if (current->header.kind == BlockKind::ClassInstance) {
            const ClassDescriptor* desc = current->header.class_desc;
            if (desc == nullptr) {
                continue;
            }
            for (const FieldSlot& field : desc->fields) {
                if (field.kind != SlotKind::Pointer) {
                    continue;
                }
                if (field.word_index >= current->payload.size()) {
                    continue;
                }
                work.push(Heap::getPointer(current, field.word_index));
            }
        } else if (current->header.kind == BlockKind::Array) {
            const ArrayDescriptor* ad = current->array.array_desc;
            if (ad == nullptr || ad->element_kind != SlotKind::Pointer) {
                continue;
            }
            for (std::size_t i = 0; i < current->array.length; i++) {
                work.push(Heap::getPointer(current, i));
            }
        }
    }
}

void GarbageCollector::markFromRoots() {
    for (HeapObject* root : roots_) {
        markObject(root);
    }
}

std::size_t GarbageCollector::collect() {
    const std::size_t before = heap_.liveCount();
    markFromRoots();
    heap_.sweepUnmarked();
    const std::size_t after = heap_.liveCount();
    return before > after ? before - after : 0;
}

HeapObject* GarbageCollector::allocClass(const ClassDescriptor* desc) {
    if (heap_.liveCount() >= threshold_objects_) {
        collect();
    }
    return heap_.allocClass(desc);
}

HeapObject* GarbageCollector::allocArray(const ArrayDescriptor* desc,
                                           std::size_t length) {
    if (heap_.liveCount() >= threshold_objects_) {
        collect();
    }
    return heap_.allocArray(desc, length);
}

}  // namespace runtime
}  // namespace compiscript
