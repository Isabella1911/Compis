#include "tac_printer.h"

#include <sstream>

namespace compiscript {
namespace ir {

std::string printOperand(const Operand& operand) {
    switch (operand.kind) {
        case OperandKind::None: return "";
        case OperandKind::Temp: return "t" + std::to_string(operand.temp);
        case OperandKind::IntConst: return std::to_string(operand.value);
        case OperandKind::Var:
        case OperandKind::String: return operand.name;
        case OperandKind::FramePtr: return "fp";
    }
    return "?";
}

namespace {

// `a` o `a op b`, la condicion de IfGoto/IfFalseGoto.
std::string conditionText(const Instruction& in) {
    if (in.op.empty()) return printOperand(in.a);
    return printOperand(in.a) + " " + in.op + " " + printOperand(in.b);
}

}  // namespace

std::string printInstruction(const Instruction& in) {
    const std::string dst = printOperand(in.dst);
    const std::string a = printOperand(in.a);
    const std::string b = printOperand(in.b);

    switch (in.code) {
        case OpCode::Copy: return dst + " = " + a;
        case OpCode::Binary: return dst + " = " + a + " " + in.op + " " + b;
        case OpCode::Unary: return dst + " = " + in.op + a;
        case OpCode::Label: return in.label + ":";
        case OpCode::Goto: return "goto " + in.label;
        case OpCode::IfGoto: return "if " + conditionText(in) + " goto " + in.label;
        case OpCode::IfFalseGoto: return "ifFalse " + conditionText(in) + " goto " + in.label;
        case OpCode::Param: return "param " + a;
        case OpCode::Link: return "link " + a;
        case OpCode::Call: {
            std::string call = "call " + in.label + ", " + std::to_string(in.argc);
            return in.dst.isNone() ? call : dst + " = " + call;
        }
        case OpCode::Return: return in.a.isNone() ? "return" : "return " + a;
        case OpCode::Load: return dst + " = " + a + "[" + b + "]";
        case OpCode::Store: return dst + "[" + a + "] = " + b;
        case OpCode::Alloc: return dst + " = alloc " + a;
        case OpCode::Print: return "print_" + in.op + " " + a;
        case OpCode::TryBegin: return "try_begin " + in.label;
        case OpCode::TryEnd: return "try_end";
        case OpCode::Comment: return "# " + in.text;
    }
    return "?";
}

std::string printProgram(const TacProgram& program) {
    std::ostringstream out;

    for (const auto& g : program.globals) {
        out << "global " << g.label << ", " << g.size << "\n";
    }
    for (const auto& s : program.strings) {
        out << "string " << s.label << ", \"" << s.value << "\"\n";
    }
    if (!program.globals.empty() || !program.strings.empty()) out << "\n";

    for (size_t i = 0; i < program.functions.size(); i++) {
        const auto& fn = program.functions[i];
        if (i > 0) out << "\n";
        out << "func " << fn.label << ", frame=" << fn.frame_size << ", temps=" << fn.max_temps
            << "\n";
        for (const auto& in : fn.code) {
            // Las etiquetas van en la columna 0 para que se distingan a simple vista.
            out << (in.code == OpCode::Label ? "" : "    ") << printInstruction(in) << "\n";
        }
        out << "endfunc\n";
    }
    return out.str();
}

}  // namespace ir
}  // namespace compiscript
