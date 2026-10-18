#include "tac.h"

#include "semantic/symbol.h"

namespace compiscript {
namespace ir {

Operand Operand::makeTemp(int index) {
    Operand o;
    o.kind = OperandKind::Temp;
    o.temp = index;
    return o;
}

Operand Operand::makeInt(int value) {
    Operand o;
    o.kind = OperandKind::IntConst;
    o.value = value;
    return o;
}

Operand Operand::makeString(const std::string& label) {
    Operand o;
    o.kind = OperandKind::String;
    o.name = label;
    return o;
}

Operand Operand::makeFramePtr() {
    Operand o;
    o.kind = OperandKind::FramePtr;
    return o;
}

Operand Operand::makeLabel(const std::string& label) {
    Operand o;
    o.kind = OperandKind::Label;
    o.name = label;
    return o;
}

Operand Operand::makeVar(semantic::Symbol* symbol) {
    Operand o;
    o.kind = OperandKind::Var;
    o.symbol = symbol;
    o.name = symbol->ir_name.empty() ? symbol->name : symbol->ir_name;
    return o;
}

}  // namespace ir
}  // namespace compiscript
