#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Instruction;
using ir::OpCode;
using ir::Operand;

ir::TacProgram TacGenerator::generate(Program& program, const FrameLayout& layout) {
    program_ = ir::TacProgram();
    contexts_.clear();
    next_label_ = 0;
    class_decls_.clear();

    for (semantic::Symbol* g : layout.globals()) program_.globals.push_back({g->ir_name, g->size});
    for (semantic::ClassSymbol* cls : layout.classes()) {
        ir::VTable vt;
        vt.label = cls->vtable_label;
        for (semantic::FunctionSymbol* m : cls->vtable) vt.methods.push_back(m->label);
        program_.vtables.push_back(std::move(vt));
    }

    // main va primero en la salida, pero termina de generarse al final
    // (despues de las funciones que declara): se le reserva el lugar 0.
    program_.functions.emplace_back();
    beginFunction("main", nullptr, layout.mainFrameSize());
    genStatements(program.statements);
    endFunction();

    return std::move(program_);
}

// ---------------------------------------------------------------------
// Contexto de funcion
// ---------------------------------------------------------------------

void TacGenerator::beginFunction(const std::string& label, semantic::FunctionSymbol* symbol,
                                 int frameSize) {
    FunctionContext ctx;
    ctx.fn.label = label;
    ctx.fn.symbol = symbol;
    ctx.fn.frame_size = frameSize;
    contexts_.push_back(std::move(ctx));
}

void TacGenerator::endFunction() {
    FunctionContext ctx = std::move(contexts_.back());
    contexts_.pop_back();
    ctx.fn.max_temps = ctx.temps.maxUsed();
    if (ctx.fn.symbol != nullptr) ctx.fn.symbol->max_temps = ctx.fn.max_temps;

    if (contexts_.empty()) {
        program_.functions[0] = std::move(ctx.fn);  // main, en su lugar reservado
    } else {
        program_.functions.push_back(std::move(ctx.fn));
    }
}

// ---------------------------------------------------------------------
// Emision
// ---------------------------------------------------------------------

void TacGenerator::emit(Instruction instruction) {
    current().fn.code.push_back(std::move(instruction));
}

void TacGenerator::emitCopy(const Operand& dst, const Operand& src) {
    Instruction in{OpCode::Copy};
    in.dst = dst;
    in.a = src;
    emit(std::move(in));
}

void TacGenerator::emitBinary(const Operand& dst, const std::string& op, const Operand& a,
                              const Operand& b) {
    Instruction in{OpCode::Binary};
    in.op = op;
    in.dst = dst;
    in.a = a;
    in.b = b;
    emit(std::move(in));
}

void TacGenerator::emitUnary(const Operand& dst, const std::string& op, const Operand& a) {
    Instruction in{OpCode::Unary};
    in.op = op;
    in.dst = dst;
    in.a = a;
    emit(std::move(in));
}

void TacGenerator::emitLabel(const std::string& label) {
    Instruction in{OpCode::Label};
    in.label = label;
    emit(std::move(in));
}

void TacGenerator::emitGoto(const std::string& label) {
    Instruction in{OpCode::Goto};
    in.label = label;
    emit(std::move(in));
}

void TacGenerator::emitIf(const Operand& cond, const std::string& label) {
    Instruction in{OpCode::IfGoto};
    in.a = cond;
    in.label = label;
    emit(std::move(in));
}

void TacGenerator::emitIfFalse(const Operand& cond, const std::string& label) {
    Instruction in{OpCode::IfFalseGoto};
    in.a = cond;
    in.label = label;
    emit(std::move(in));
}

void TacGenerator::emitIfRel(const Operand& a, const std::string& op, const Operand& b,
                             const std::string& label) {
    Instruction in{OpCode::IfGoto};
    in.op = op;
    in.a = a;
    in.b = b;
    in.label = label;
    emit(std::move(in));
}

void TacGenerator::emitIfFalseRel(const Operand& a, const std::string& op, const Operand& b,
                                  const std::string& label) {
    Instruction in{OpCode::IfFalseGoto};
    in.op = op;
    in.a = a;
    in.b = b;
    in.label = label;
    emit(std::move(in));
}

void TacGenerator::emitParam(const Operand& a) {
    Instruction in{OpCode::Param};
    in.a = a;
    emit(std::move(in));
}

void TacGenerator::emitLink(const Operand& a) {
    Instruction in{OpCode::Link};
    in.a = a;
    emit(std::move(in));
}

Operand TacGenerator::emitCall(const std::string& label, int argc, bool hasResult) {
    Instruction in{OpCode::Call};
    in.label = label;
    in.argc = argc;
    if (hasResult) in.dst = newTemp();
    Operand result = in.dst;
    emit(std::move(in));
    return result;
}

Operand TacGenerator::emitCallIndirect(const Operand& target, int argc, bool hasResult) {
    Instruction in{OpCode::Call};
    in.a = target;
    in.argc = argc;
    release(target);  // se lee antes de escribir el resultado: puede reusarse
    if (hasResult) in.dst = newTemp();
    Operand result = in.dst;
    emit(std::move(in));
    return result;
}

void TacGenerator::emitReturn(const Operand& value) {
    Instruction in{OpCode::Return};
    in.a = value;
    emit(std::move(in));
}

Operand TacGenerator::emitLoad(const Operand& base, const Operand& offset) {
    Instruction in{OpCode::Load};
    in.dst = newTemp();
    in.a = base;
    in.b = offset;
    Operand result = in.dst;
    emit(std::move(in));
    return result;
}

void TacGenerator::emitStore(const Operand& base, const Operand& offset, const Operand& value) {
    Instruction in{OpCode::Store};
    in.dst = base;
    in.a = offset;
    in.b = value;
    emit(std::move(in));
}

Operand TacGenerator::emitAlloc(const Operand& bytes) {
    Instruction in{OpCode::Alloc};
    in.dst = newTemp();
    in.a = bytes;
    Operand result = in.dst;
    emit(std::move(in));
    return result;
}

void TacGenerator::emitPrint(const std::string& kind, const Operand& a) {
    Instruction in{OpCode::Print};
    in.op = kind;
    in.a = a;
    emit(std::move(in));
}

void TacGenerator::emitComment(const std::string& text) {
    Instruction in{OpCode::Comment};
    in.text = text;
    emit(std::move(in));
}

// ---------------------------------------------------------------------
// Temporales, etiquetas, strings, variables, bucles
// ---------------------------------------------------------------------

Operand TacGenerator::newTemp() { return current().temps.acquire(); }

void TacGenerator::release(const Operand& operand) { current().temps.release(operand); }

std::string TacGenerator::newLabel() { return "L" + std::to_string(next_label_++); }

Operand TacGenerator::internString(const std::string& value) {
    for (const auto& s : program_.strings) {
        if (s.value == value) return Operand::makeString(s.label);
    }
    std::string label = "str_" + std::to_string(program_.strings.size());
    program_.strings.push_back({label, value});
    return Operand::makeString(label);
}

Operand TacGenerator::varOperand(semantic::Symbol* symbol) { return Operand::makeVar(symbol); }

bool TacGenerator::isCaptured(semantic::Symbol* symbol) const {
    // ClosureAnalyzer cuenta como capturada tambien a una global; esas no
    // necesitan enlace de acceso (viven en g_<nombre>), asi que se excluyen.
    if (symbol->storage != semantic::StorageKind::Local &&
        symbol->storage != semantic::StorageKind::Param) {
        return false;
    }
    const semantic::FunctionSymbol* fn = contexts_.back().fn.symbol;
    if (fn == nullptr) return false;
    for (const semantic::Symbol* c : fn->captured) {
        if (c == symbol) return true;
    }
    return false;
}

Operand TacGenerator::loadVar(semantic::Symbol* symbol) {
    if (isCaptured(symbol)) return genCapturedLoad(symbol);
    return varOperand(symbol);
}

void TacGenerator::storeVar(semantic::Symbol* symbol, const Operand& value) {
    if (isCaptured(symbol)) {
        genCapturedStore(symbol, value);
        release(value);
        return;
    }
    assignTo(varOperand(symbol), value);
}

void TacGenerator::assignTo(const Operand& dst, const Operand& value) {
    auto& code = current().fn.code;
    if (value.isTemp() && !code.empty()) {
        Instruction& last = code.back();
        bool producesValue = last.code == OpCode::Copy || last.code == OpCode::Binary ||
                             last.code == OpCode::Unary || last.code == OpCode::Load ||
                             last.code == OpCode::Call || last.code == OpCode::Alloc;
        // Un temporal se consume una sola vez, asi que si lo acaba de
        // escribir la instruccion anterior nadie mas lo va a leer.
        if (producesValue && last.dst.isTemp() && last.dst.temp == value.temp) {
            last.dst = dst;
            release(value);
            return;
        }
    }
    emitCopy(dst, value);
    release(value);
}

void TacGenerator::pushLoop(const std::string& breakLabel, const std::string& continueLabel) {
    current().loops.push_back({breakLabel, continueLabel, current().try_depth});
}

void TacGenerator::popLoop() { current().loops.pop_back(); }

const TacGenerator::LoopLabels* TacGenerator::currentLoop() const {
    const auto& loops = contexts_.back().loops;
    return loops.empty() ? nullptr : &loops.back();
}

void TacGenerator::closeTryHandlers(int depth) {
    for (int d = current().try_depth; d > depth; d--) {
        emit(Instruction{OpCode::TryEnd});
    }
}

Operand TacGenerator::todo(const std::string& owner, const std::string& what) {
    emitComment("TODO(" + owner + "): " + what);
    return Operand::makeInt(0);
}

// ---------------------------------------------------------------------
// Despacho por tipo de nodo
// ---------------------------------------------------------------------

void TacGenerator::genStatements(const std::vector<StatementPtr>& statements) {
    for (const auto& stmt : statements) genStatement(stmt.get());
}

void TacGenerator::genStatement(Statement* stmt) {
    if (auto* n = dynamic_cast<VariableDeclaration*>(stmt)) return genVariableDeclaration(n);
    if (auto* n = dynamic_cast<ConstantDeclaration*>(stmt)) return genConstantDeclaration(n);
    if (auto* n = dynamic_cast<AssignmentStatement*>(stmt)) return genAssignmentStatement(n);
    if (auto* n = dynamic_cast<PropertyAssignment*>(stmt)) {
        release(genPropertyAssignment(n->object.get(), n->symbol, n->value.get()));
        return;
    }
    if (auto* n = dynamic_cast<ExpressionStatement*>(stmt)) return genExpressionStatement(n);
    if (auto* n = dynamic_cast<PrintStatement*>(stmt)) return genPrint(n);
    if (auto* n = dynamic_cast<Block*>(stmt)) return genBlock(n);
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) return genIf(n);
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) return genWhile(n);
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) return genDoWhile(n);
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) return genFor(n);
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) return genForeach(n);
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) return genSwitch(n);
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) return genTryCatch(n);
    if (auto* n = dynamic_cast<ReturnStatement*>(stmt)) return genReturn(n);
    if (auto* n = dynamic_cast<BreakStatement*>(stmt)) return genBreak(n);
    if (auto* n = dynamic_cast<ContinueStatement*>(stmt)) return genContinue(n);
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) return genFunctionDeclaration(n);
    if (auto* n = dynamic_cast<ClassDeclaration*>(stmt)) return genClassDeclaration(n);
    todo("parte 1", "sentencia no reconocida");
}

Operand TacGenerator::genExpr(Expression* expr) {
    if (auto* n = dynamic_cast<LiteralExpression*>(expr)) return genLiteral(n);
    if (auto* n = dynamic_cast<IdentifierExpression*>(expr)) return genIdentifier(n);
    if (auto* n = dynamic_cast<BinaryExpression*>(expr)) return genBinary(n);
    if (auto* n = dynamic_cast<UnaryExpression*>(expr)) return genUnary(n);
    if (auto* n = dynamic_cast<AssignmentExpression*>(expr)) return genAssignment(n);
    if (auto* n = dynamic_cast<PropertyAssignExpr*>(expr)) {
        return genPropertyAssignment(n->object.get(), n->symbol, n->value.get());
    }
    if (auto* n = dynamic_cast<TernaryExpression*>(expr)) return genTernary(n);
    if (auto* n = dynamic_cast<CallExpression*>(expr)) return genCall(n);
    if (auto* n = dynamic_cast<NewExpression*>(expr)) return genNew(n);
    if (auto* n = dynamic_cast<MemberAccessExpression*>(expr)) return genMemberAccess(n);
    if (auto* n = dynamic_cast<ThisExpression*>(expr)) return genThis(n);
    if (auto* n = dynamic_cast<ArrayLiteral*>(expr)) return genArrayLiteral(n);
    if (auto* n = dynamic_cast<ArrayAccessExpression*>(expr)) return genArrayAccess(n);
    return todo("parte 1", "expresion no reconocida");
}

}  // namespace codegen
}  // namespace compiscript
