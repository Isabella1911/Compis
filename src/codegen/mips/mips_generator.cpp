#include "mips_generator.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "diagnostics/codes.h"
#include "mips_emitter.h"
#include "semantic/symbol.h"

namespace compiscript {
namespace codegen {
namespace mips {

namespace {

using diagnostics::Diagnostic;
using diagnostics::Severity;
namespace codes = diagnostics::codes;
using ir::Instruction;
using ir::OpCode;
using ir::Operand;
using ir::OperandKind;
using ir::TacFunction;
using ir::TacProgram;
using semantic::StorageKind;

// Layout alineado con FrameLayout / docs/08:
//   fp+8+4i  param i
//   fp+4     static link (siempre reservado por el caller; 0 si no hay)
//   fp+0     old fp
//   fp-4     ra
//   fp-8-4k  local k
//   debajo   temps: temp t en fp - (8 + frame_size + 4*t)

struct Gen {
    MipsEmitter em;
    std::vector<Diagnostic> diags;
    bool failed = false;

    const TacFunction* fn = nullptr;
    // Argumentos y enlace de la llamada en curso, ya apilados. Se cuentan
    // para validar contra el argc del call y para limpiar la pila despues.
    int pushedParams = 0;
    bool hasLink = false;

    std::unordered_set<std::string> definedLabels;
    std::unordered_set<std::string> usedLabels;

    void error(const char* code, const std::string& message) {
        failed = true;
        Diagnostic d;
        d.severity = Severity::Error;
        d.code = code;
        d.message = message;
        diags.push_back(d);
    }

    int tempOffset(int t) const {
        return -(8 + fn->frame_size + 4 * t);
    }

    // Direccion "off(base)" para lw/sw. Si el desplazamiento no cabe en los
    // 16 bits con signo de la instruccion, la direccion se arma en $t9 (que
    // no se usa para nada mas) y se devuelve "0($t9)".
    std::string memRef(int offset, const std::string& base) {
        if (offset >= -32768 && offset <= 32767) {
            return std::to_string(offset) + "(" + base + ")";
        }
        em.insn("li $t9, " + std::to_string(offset));
        em.insn("addu $t9, $t9, " + base);
        return "0($t9)";
    }

    void loadOperand(const Operand& op, const char* reg) {
        switch (op.kind) {
            case OperandKind::None:
                error(codes::CG002, "Operando ausente donde se esperaba un valor");
                em.insn(std::string("li ") + reg + ", 0");
                break;
            case OperandKind::IntConst:
                em.insn(std::string("li ") + reg + ", " + std::to_string(op.value));
                break;
            case OperandKind::Temp:
                em.insn(std::string("lw ") + reg + ", " + memRef(tempOffset(op.temp), "$fp"));
                break;
            case OperandKind::Var:
                if (op.symbol == nullptr) {
                    error(codes::CG002, "Variable TAC sin Symbol: " + op.name);
                    em.insn(std::string("li ") + reg + ", 0");
                    break;
                }
                if (op.symbol->storage == StorageKind::Global) {
                    em.insn(std::string("lw ") + reg + ", " + op.name);
                } else if (op.symbol->storage == StorageKind::Local ||
                           op.symbol->storage == StorageKind::Param) {
                    em.insn(std::string("lw ") + reg + ", " + memRef(op.symbol->offset, "$fp"));
                } else {
                    error(codes::CG004,
                          "Acceso a campo/objeto aún no soportado en MIPS: " + op.name);
                    em.insn(std::string("li ") + reg + ", 0");
                }
                break;
            case OperandKind::String:
                em.insn(std::string("la ") + reg + ", " + op.name);
                break;
            case OperandKind::FramePtr:
                em.insn(std::string("move ") + reg + ", $fp");
                break;
            case OperandKind::Label:
                em.insn(std::string("la ") + reg + ", " + op.name);
                usedLabels.insert(op.name);
                break;
        }
    }

    void storeOperand(const Operand& dst, const char* reg) {
        switch (dst.kind) {
            case OperandKind::Temp:
                em.insn(std::string("sw ") + reg + ", " + memRef(tempOffset(dst.temp), "$fp"));
                break;
            case OperandKind::Var:
                if (dst.symbol == nullptr) {
                    error(codes::CG002, "Destino variable sin Symbol: " + dst.name);
                    break;
                }
                if (dst.symbol->storage == StorageKind::Global) {
                    em.insn(std::string("sw ") + reg + ", " + dst.name);
                } else if (dst.symbol->storage == StorageKind::Local ||
                           dst.symbol->storage == StorageKind::Param) {
                    em.insn(std::string("sw ") + reg + ", " + memRef(dst.symbol->offset, "$fp"));
                } else {
                    error(codes::CG004,
                          "Asignación a campo/objeto aún no soportada en MIPS: " + dst.name);
                }
                break;
            default:
                error(codes::CG002, "Destino de asignación inválido en TAC");
                break;
        }
    }

    void emitBinaryArith(const std::string& op) {
        if (op == "+") em.insn("addu $t0, $t0, $t1");
        else if (op == "-") em.insn("subu $t0, $t0, $t1");
        else if (op == "*") em.insn("mul $t0, $t0, $t1");
        else if (op == "/") {
            em.insn("beq $t1, $zero, __div_zero");
            em.insn("div $t0, $t1");
            em.insn("mflo $t0");
        } else if (op == "%") {
            em.insn("beq $t1, $zero, __div_zero");
            em.insn("div $t0, $t1");
            em.insn("mfhi $t0");
        } else if (op == "==") em.insn("seq $t0, $t0, $t1");
        else if (op == "!=") em.insn("sne $t0, $t0, $t1");
        else if (op == "<") em.insn("slt $t0, $t0, $t1");
        else if (op == "<=") em.insn("sle $t0, $t0, $t1");
        else if (op == ">") em.insn("sgt $t0, $t0, $t1");
        else if (op == ">=") em.insn("sge $t0, $t0, $t1");
        else error(codes::CG001, "Operador binario TAC no soportado: '" + op + "'");
    }

    void emitCompareBranch(const std::string& op, bool ifFalse, const std::string& label) {
        usedLabels.insert(label);
        std::string cond = op;
        if (ifFalse) {
            if (op == "==") cond = "!=";
            else if (op == "!=") cond = "==";
            else if (op == "<") cond = ">=";
            else if (op == "<=") cond = ">";
            else if (op == ">") cond = "<=";
            else if (op == ">=") cond = "<";
            else {
                error(codes::CG001, "Comparación TAC no soportada: '" + op + "'");
                return;
            }
        }
        if (cond == "==") em.insn("beq $t0, $t1, " + label);
        else if (cond == "!=") em.insn("bne $t0, $t1, " + label);
        else if (cond == "<") em.insn("blt $t0, $t1, " + label);
        else if (cond == "<=") em.insn("ble $t0, $t1, " + label);
        else if (cond == ">") em.insn("bgt $t0, $t1, " + label);
        else if (cond == ">=") em.insn("bge $t0, $t1, " + label);
        else error(codes::CG001, "Comparación TAC no soportada: '" + op + "'");
    }

    void emitPrologue() {
        int frameBytes = fn->frame_size + fn->max_temps * 4;
        em.label(fn->label);
        // Entrada: $sp apunta al static link (siempre presente; ver emitCall).
        em.insn("addiu $sp, $sp, -4");
        em.insn("sw $fp, 0($sp)");        // old fp; link queda en 4($sp)
        em.insn("addiu $sp, $sp, -4");
        em.insn("sw $ra, 0($sp)");        // ra
        em.insn("addiu $fp, $sp, 4");     // fp → old fp; ra=-4; link=4; params=8+
        if (frameBytes > 0) {
            em.insn("addiu $sp, $sp, -" + std::to_string(frameBytes));
        }
    }

    // Deja $sp exactamente como estaba al entrar (apuntando al enlace): con
    // $sp = $fp, debajo queda $ra (ya leido) y en 0($fp) el fp viejo, asi
    // que solo hay que descartar UNA palabra. El enlace y los argumentos los
    // limpia el llamador (ver emitCall).
    void emitEpilogue() {
        em.insn("move $sp, $fp");
        em.insn("lw $ra, -4($fp)");
        em.insn("lw $fp, 0($fp)");
        em.insn("addiu $sp, $sp, 4");
        em.insn("jr $ra");
        em.insn("nop");
    }

    void push(const char* reg) {
        em.insn("addiu $sp, $sp, -4");
        em.insn(std::string("sw ") + reg + ", 0($sp)");
    }

    // param x: el valor se apila EN ESTE MOMENTO, no al llegar al call. El
    // TAC reutiliza temporales entre el param y el call: en una llamada por
    // vtable hace "param t1" (this) y despues "t1 = t1[0]" para buscar el
    // metodo, asi que leer t1 recien en el call pasaria la direccion del
    // metodo en lugar del objeto.
    void emitParam(const Operand& a) {
        loadOperand(a, "$t0");
        push("$t0");
        ++pushedParams;
    }

    // link x: el TAC lo emite justo antes del call, despues de los param,
    // asi que queda en fp+4 del llamado.
    void emitLink(const Operand& a) {
        loadOperand(a, "$t0");
        push("$t0");
        hasLink = true;
    }

    // call f, n (directa: jal) o call t, n (indirecta: jalr, la direccion
    // sale de la vtable). Pila al entrar al llamado:
    //   sp+0  static link (0 si no hay)   ->  fp+4 del llamado
    //   sp+4  primer argumento            ->  fp+8
    //   ...
    // El llamador limpia argumentos y enlace al volver.
    void emitCall(const Instruction& in) {
        if (pushedParams != in.argc) {
            error(codes::CG002, "call con " + std::to_string(in.argc) + " argumento(s) pero " +
                                    std::to_string(pushedParams) + " param antes del call");
        }
        if (!hasLink) {
            push("$zero");
        }
        if (in.label.empty()) {
            loadOperand(in.a, "$t3");
            em.insn("jalr $t3");
        } else {
            usedLabels.insert(in.label);
            em.insn("jal " + in.label);
        }
        em.insn("nop");

        int cleanup = in.argc * 4 + 4;  // argumentos + enlace
        em.insn("addiu $sp, $sp, " + std::to_string(cleanup));
        pushedParams = 0;
        hasLink = false;

        if (!in.dst.isNone()) storeOperand(in.dst, "$v0");
    }

    // Fin de main: comprueba que la pila quedo exactamente donde estaba al
    // terminar el prologo (todas las llamadas limpiaron lo que apilaron) y
    // termina el programa con codigo 0.
    void emitMainExit() {
        usedLabels.insert("__stack_error");
        em.insn("lw $t0, __sp_main");
        em.insn("bne $t0, $sp, __stack_error");
        em.insn("li $v0, 10");
        em.insn("syscall");
    }

    // Deja en $t0 la direccion base de un acceso a memoria del TAC
    // (dst = a[b] o dst[a] = b) y devuelve el "off(reg)" listo para lw/sw.
    // La base puede ser un objeto, una lista o fp (closures: fp[4] es el
    // enlace de acceso). Si la base no es fp y vale 0 es una referencia nula:
    // se salta al runtime (__null_ref) en lugar de leer la direccion 0.
    std::string heapAddress(const Operand& base, const Operand& offset) {
        loadOperand(base, "$t0");
        if (base.kind != OperandKind::FramePtr) {
            usedLabels.insert("__null_ref");
            em.insn("beq $t0, $zero, __null_ref");
        }
        if (offset.kind == OperandKind::IntConst) return memRef(offset.value, "$t0");
        loadOperand(offset, "$t1");
        em.insn("addu $t0, $t0, $t1");
        return "0($t0)";
    }

    // dst = alloc n: syscall 9 (sbrk). El tamano se redondea a multiplo de 4
    // para que el siguiente bloque del heap tambien quede alineado a palabra
    // (las cadenas de __concat piden tamanos que no son multiplo de 4).
    void emitAlloc(const Operand& dst, const Operand& size) {
        loadOperand(size, "$a0");
        em.insn("addiu $a0, $a0, 3");
        em.insn("li $t1, -4");
        em.insn("and $a0, $a0, $t1");
        em.insn("li $v0, 9");
        em.insn("syscall");
        storeOperand(dst, "$v0");
    }

    void emitPrint(const Instruction& in, int& boolLabel) {
        loadOperand(in.a, "$a0");
        if (in.op == "int") {
            em.insn("li $v0, 1");
            em.insn("syscall");
        } else if (in.op == "str") {
            em.insn("li $v0, 4");
            em.insn("syscall");
        } else if (in.op == "bool") {
            int id = boolLabel++;
            std::string elseL = "__bool_false_" + std::to_string(id);
            std::string endL = "__bool_end_" + std::to_string(id);
            em.insn("beq $a0, $zero, " + elseL);
            em.insn("la $a0, __true_str");
            em.insn("j " + endL);
            em.label(elseL);
            em.insn("la $a0, __false_str");
            em.label(endL);
            em.insn("li $v0, 4");
            em.insn("syscall");
        } else {
            error(codes::CG001, "print_" + in.op + " no soportado");
            return;
        }
        // Newline para que la salida MARS sea comparable en tests.
        em.insn("li $a0, 10");
        em.insn("li $v0, 11");
        em.insn("syscall");
    }

    void translateInsn(const Instruction& in, int& boolLabel) {
        switch (in.code) {
            case OpCode::Comment:
                em.comment(in.text);
                break;
            case OpCode::Label:
                definedLabels.insert(in.label);
                em.label(in.label);
                break;
            case OpCode::Copy:
                loadOperand(in.a, "$t0");
                storeOperand(in.dst, "$t0");
                break;
            case OpCode::Unary:
                loadOperand(in.a, "$t0");
                if (in.op == "-") em.insn("negu $t0, $t0");
                else if (in.op == "!") em.insn("seq $t0, $t0, $zero");
                else error(codes::CG001, "Operador unario no soportado: '" + in.op + "'");
                storeOperand(in.dst, "$t0");
                break;
            case OpCode::Binary:
                loadOperand(in.a, "$t0");
                loadOperand(in.b, "$t1");
                emitBinaryArith(in.op);
                storeOperand(in.dst, "$t0");
                break;
            case OpCode::Goto:
                usedLabels.insert(in.label);
                em.insn("j " + in.label);
                em.insn("nop");
                break;
            case OpCode::IfGoto:
                if (in.op.empty()) {
                    loadOperand(in.a, "$t0");
                    usedLabels.insert(in.label);
                    em.insn("bnez $t0, " + in.label);
                } else {
                    loadOperand(in.a, "$t0");
                    loadOperand(in.b, "$t1");
                    emitCompareBranch(in.op, false, in.label);
                }
                break;
            case OpCode::IfFalseGoto:
                if (in.op.empty()) {
                    loadOperand(in.a, "$t0");
                    usedLabels.insert(in.label);
                    em.insn("beqz $t0, " + in.label);
                } else {
                    loadOperand(in.a, "$t0");
                    loadOperand(in.b, "$t1");
                    emitCompareBranch(in.op, true, in.label);
                }
                break;
            case OpCode::Param:
                emitParam(in.a);
                break;
            case OpCode::Link:
                emitLink(in.a);
                break;
            case OpCode::Call:
                emitCall(in);
                break;
            case OpCode::Return:
                if (!in.a.isNone()) loadOperand(in.a, "$v0");
                if (fn->label == "main") {
                    emitMainExit();
                } else {
                    emitEpilogue();
                }
                break;
            case OpCode::Print:
                emitPrint(in, boolLabel);
                break;
            case OpCode::Load: {  // dst = a[b]
                std::string addr = heapAddress(in.a, in.b);
                em.insn("lw $t1, " + addr);
                storeOperand(in.dst, "$t1");
                break;
            }
            case OpCode::Store: {  // dst[a] = b
                // El valor va primero a $t2: heapAddress usa $t0, $t1 y $t9.
                loadOperand(in.b, "$t2");
                std::string addr = heapAddress(in.dst, in.a);
                em.insn("sw $t2, " + addr);
                break;
            }
            case OpCode::Alloc:  // dst = alloc a
                emitAlloc(in.dst, in.a);
                break;
            case OpCode::TryBegin:
            case OpCode::TryEnd:
                error(codes::CG004, "try/catch aún no soportado en el backend MIPS");
                break;
        }
    }

    void emitData(const TacProgram& program) {
        em.directive(".data");
        em.directive(".align 2");
        for (const auto& g : program.globals) {
            em.label(g.label);
            em.directive("    .word 0");
        }
        for (const auto& s : program.strings) {
            em.label(s.label);
            em.directive("    .asciiz \"" + escapeAsciiz(s.value) + "\"");
        }
        for (const auto& vt : program.vtables) {
            // Las vtables se emiten para no romper etiquetas; el dispatch
            // indirecto todavia no esta soportado (CG004 en Call vacio).
            em.label(vt.label);
            if (vt.methods.empty()) {
                em.directive("    .word 0");
            } else {
                for (const auto& m : vt.methods) {
                    em.directive("    .word " + m);
                    usedLabels.insert(m);
                }
            }
        }
        em.label("__true_str");
        em.directive("    .asciiz \"true\"");
        em.label("__false_str");
        em.directive("    .asciiz \"false\"");
        em.label("__error_prefix");
        em.directive("    .asciiz \"Error en tiempo de ejecucion: \"");
        em.label("__divzero_msg");
        em.directive("    .asciiz \"division entre cero\"");
        em.label("__null_msg");
        em.directive("    .asciiz \"referencia nula\"");
        em.label("__runtime_msg");
        em.directive("    .asciiz \"error interno\"");
        em.label("__stack_msg");
        em.directive("    .asciiz \"pila desbalanceada al terminar main\"");
        em.directive(".align 2");
        em.label("__sp_main");  // $sp de main despues del prologo
        em.directive("    .word 0");
        em.blank();
    }

    void emitRuntime() {
        em.comment("--- runtime builtins ---");
        // Args: 8($fp)=left (ultimo push), 12($fp)=right. Ver emitCall + RTL.
        em.label("__concat");
        emitPrologueForBuiltin();
        em.insn("lw $a0, 8($fp)");
        em.insn("lw $a1, 12($fp)");
        em.insn("move $t0, $a0");
        em.label("__concat_len0");
        em.insn("lb $t1, 0($t0)");
        em.insn("beq $t1, $zero, __concat_len0_done");
        em.insn("addiu $t0, $t0, 1");
        em.insn("j __concat_len0");
        em.label("__concat_len0_done");
        em.insn("subu $t2, $t0, $a0");
        em.insn("move $t0, $a1");
        em.label("__concat_len1");
        em.insn("lb $t1, 0($t0)");
        em.insn("beq $t1, $zero, __concat_len1_done");
        em.insn("addiu $t0, $t0, 1");
        em.insn("j __concat_len1");
        em.label("__concat_len1_done");
        em.insn("subu $t3, $t0, $a1");
        em.insn("addu $a0, $t2, $t3");
        em.insn("addiu $a0, $a0, 4");  // +1 del terminador, redondeado a multiplo de 4
        em.insn("li $t1, -4");
        em.insn("and $a0, $a0, $t1");
        em.insn("li $v0, 9");
        em.insn("syscall");
        em.insn("move $t4, $v0");
        em.insn("lw $a0, 8($fp)");
        em.insn("move $t0, $t4");
        em.label("__concat_copy0");
        em.insn("lb $t1, 0($a0)");
        em.insn("beq $t1, $zero, __concat_copy0_done");
        em.insn("sb $t1, 0($t0)");
        em.insn("addiu $a0, $a0, 1");
        em.insn("addiu $t0, $t0, 1");
        em.insn("j __concat_copy0");
        em.label("__concat_copy0_done");
        em.insn("lw $a1, 12($fp)");
        em.label("__concat_copy1");
        em.insn("lb $t1, 0($a1)");
        em.insn("sb $t1, 0($t0)");
        em.insn("beq $t1, $zero, __concat_done");
        em.insn("addiu $a1, $a1, 1");
        em.insn("addiu $t0, $t0, 1");
        em.insn("j __concat_copy1");
        em.label("__concat_done");
        em.insn("move $v0, $t4");
        emitEpilogue();

        em.label("__str_eq");
        emitPrologueForBuiltin();
        em.insn("lw $a0, 8($fp)");
        em.insn("lw $a1, 12($fp)");
        em.label("__str_eq_loop");
        em.insn("lb $t0, 0($a0)");
        em.insn("lb $t1, 0($a1)");
        em.insn("bne $t0, $t1, __str_eq_no");
        em.insn("beq $t0, $zero, __str_eq_yes");
        em.insn("addiu $a0, $a0, 1");
        em.insn("addiu $a1, $a1, 1");
        em.insn("j __str_eq_loop");
        em.label("__str_eq_yes");
        em.insn("li $v0, 1");
        em.insn("j __str_eq_end");
        em.label("__str_eq_no");
        em.insn("li $v0, 0");
        em.label("__str_eq_end");
        emitEpilogue();

        // Errores de runtime. Todos terminan en __fail con el mensaje en $a0:
        //   __raise          lo llama el TAC (param msg; call __raise, 1), p. ej.
        //                    con "indice fuera de rango"
        //   __div_zero       division o modulo entre cero
        //   __null_ref       acceso a un campo o elemento de una referencia nula
        //   __runtime_error  error interno
        em.label("__raise");
        emitPrologueForBuiltin();
        em.insn("lw $a0, 8($fp)");
        em.insn("j __fail");
        em.insn("nop");

        em.label("__div_zero");
        em.insn("la $a0, __divzero_msg");
        em.insn("j __fail");
        em.insn("nop");

        em.label("__null_ref");
        em.insn("la $a0, __null_msg");
        em.insn("j __fail");
        em.insn("nop");

        em.label("__runtime_error");
        em.insn("la $a0, __runtime_msg");
        em.insn("j __fail");
        em.insn("nop");

        em.label("__stack_error");
        em.insn("la $a0, __stack_msg");
        em.insn("j __fail");
        em.insn("nop");

        // Imprime "Error en tiempo de ejecucion: <mensaje>" y termina con
        // codigo de salida 1 (syscall 17), distinto del fin normal (0).
        em.label("__fail");
        em.insn("move $t0, $a0");
        em.insn("la $a0, __error_prefix");
        em.insn("li $v0, 4");
        em.insn("syscall");
        em.insn("move $a0, $t0");
        em.insn("li $v0, 4");
        em.insn("syscall");
        em.insn("li $a0, 10");
        em.insn("li $v0, 11");
        em.insn("syscall");
        em.insn("li $a0, 1");
        em.insn("li $v0, 17");
        em.insn("syscall");
    }

    // Builtin: no usa fn->frame; prologo minimo compatible con fp+8 args.
    void emitPrologueForBuiltin() {
        em.insn("addiu $sp, $sp, -4");
        em.insn("sw $fp, 0($sp)");
        em.insn("addiu $sp, $sp, -4");
        em.insn("sw $ra, 0($sp)");
        em.insn("addiu $fp, $sp, 4");
    }

    void translateFunction(const TacFunction& f) {
        fn = &f;
        pushedParams = 0;
        hasLink = false;
        int boolLabel = 0;

        // main tambien recibe un "link" dummy: MARS salta a main sin args.
        // Empujamos un 0 para que el prologo vea la misma forma.
        if (f.label == "main") {
            em.label("main");
            em.insn("addiu $sp, $sp, -4");
            em.insn("sw $zero, 0($sp)");  // static link dummy
            // Reusar emitPrologue necesita fn.label == main already labeled —
            // emitir cuerpo del prologo sin repetir label.
            int frameBytes = f.frame_size + f.max_temps * 4;
            em.insn("addiu $sp, $sp, -4");
            em.insn("sw $fp, 0($sp)");
            em.insn("addiu $sp, $sp, -4");
            em.insn("sw $ra, 0($sp)");
            em.insn("addiu $fp, $sp, 4");
            if (frameBytes > 0) {
                em.insn("addiu $sp, $sp, -" + std::to_string(frameBytes));
            }
            em.insn("sw $sp, __sp_main");
            for (const auto& in : f.code) {
                if (failed) break;
                translateInsn(in, boolLabel);
            }
            emitMainExit();
        } else {
            emitPrologue();
            bool sawReturn = false;
            for (const auto& in : f.code) {
                if (failed) break;
                if (in.code == OpCode::Return) sawReturn = true;
                translateInsn(in, boolLabel);
            }
            if (!sawReturn) emitEpilogue();
        }
        fn = nullptr;
    }

    void checkLabels(const TacProgram& program) {
        for (const auto& f : program.functions) definedLabels.insert(f.label);
        for (const auto& g : program.globals) definedLabels.insert(g.label);
        for (const auto& s : program.strings) definedLabels.insert(s.label);
        for (const auto& vt : program.vtables) definedLabels.insert(vt.label);
        definedLabels.insert("__concat");
        definedLabels.insert("__str_eq");
        definedLabels.insert("__div_zero");
        definedLabels.insert("__runtime_error");
        definedLabels.insert("__raise");
        definedLabels.insert("__null_ref");
        definedLabels.insert("__fail");
        definedLabels.insert("__stack_error");
        definedLabels.insert("__sp_main");
        definedLabels.insert("__true_str");
        definedLabels.insert("__false_str");

        for (const auto& u : usedLabels) {
            if (!definedLabels.count(u)) {
                error(codes::CG003, "Etiqueta indefinida en MIPS: " + u);
            }
        }
    }
};

}  // namespace

MipsResult generateMips(const TacProgram& program) {
    Gen g;
    g.emitData(program);
    g.em.directive(".text");
    g.em.directive(".globl main");
    g.em.blank();

    for (const auto& f : program.functions) {
        if (g.failed) break;
        g.translateFunction(f);
        g.em.blank();
    }

    if (!g.failed) g.emitRuntime();
    g.checkLabels(program);

    MipsResult result;
    result.diagnostics = std::move(g.diags);
    result.success = !g.failed;
    if (result.success) result.assembly = g.em.str();
    return result;
}

}  // namespace mips
}  // namespace codegen
}  // namespace compiscript
