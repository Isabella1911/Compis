#include "temp_allocator.h"

namespace compiscript {
namespace codegen {

ir::Operand TempAllocator::acquire() {
    if (!free_.empty()) {
        int index = *free_.begin();
        free_.erase(free_.begin());
        return ir::Operand::makeTemp(index);
    }
    return ir::Operand::makeTemp(next_++);
}

void TempAllocator::release(const ir::Operand& operand) {
    if (!operand.isTemp()) return;
    free_.insert(operand.temp);
}

}  // namespace codegen
}  // namespace compiscript
