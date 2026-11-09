// Pruebas unitarias del backend MIPS: arman un ir::TacProgram a mano y
// llaman directamente a codegen::mips::generateMips, sin pasar por el
// frontend. Asi se pueden probar casos que el frontend nunca genera (TAC
// malformado, frames imposibles, offsets enormes) y aislar el backend.
//
// Dos clases de casos:
//   - de error: el generador tiene que rechazarlos con el codigo CGxxx
//     esperado y SIN ensamblador (ni siquiera parcial);
//   - ejecutables: ademas de revisar propiedades del .asm (registros
//     reservados, jalr, direcciones fuera de 16 bits...), con --emitir DIR
//     se escriben DIR/<caso>.asm, .out (salida esperada) y .exit; el script
//     tests/mips/unit/run_unit.py los ejecuta en MARS y compara. Lo que vale
//     es el resultado en el simulador, no el texto exacto del .asm.
//
// Uso: test_mips_backend [--emitir DIR]

#include <deque>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "codegen/mips/mips_emitter.h"
#include "codegen/mips/mips_generator.h"
#include "diagnostics/codes.h"
#include "ir/tac.h"
#include "semantic/symbol.h"

using namespace compiscript;
using ir::Instruction;
using ir::OpCode;
using ir::Operand;
using ir::TacFunction;
using ir::TacProgram;
using semantic::StorageKind;
namespace mips = codegen::mips;
namespace codes = diagnostics::codes;

namespace {

int total = 0;
int fallos = 0;

void check(bool ok, const std::string& caso, const std::string& que) {
    ++total;
    if (!ok) {
        ++fallos;
        std::cout << "FALLO  " << caso << ": " << que << "\n";
    }
}

// --- Constructores del TAC -------------------------------------------------

Operand T(int i) { return Operand::makeTemp(i); }
Operand K(int v) { return Operand::makeInt(v); }
Operand S(const std::string& l) { return Operand::makeString(l); }
Operand L(const std::string& l) { return Operand::makeLabel(l); }
Operand FP() { return Operand::makeFramePtr(); }
Operand NONE() { return Operand::none(); }

Instruction I(OpCode c) {
    Instruction i{};
    i.code = c;
    return i;
}
Instruction copy(Operand d, Operand a) {
    auto i = I(OpCode::Copy);
    i.dst = d;
    i.a = a;
    return i;
}
Instruction bin(const std::string& op, Operand d, Operand a, Operand b) {
    auto i = I(OpCode::Binary);
    i.op = op;
    i.dst = d;
    i.a = a;
    i.b = b;
    return i;
}
Instruction un(const std::string& op, Operand d, Operand a) {
    auto i = I(OpCode::Unary);
    i.op = op;
    i.dst = d;
    i.a = a;
    return i;
}
Instruction lab(const std::string& l) {
    auto i = I(OpCode::Label);
    i.label = l;
    return i;
}
Instruction go(const std::string& l) {
    auto i = I(OpCode::Goto);
    i.label = l;
    return i;
}
// ifFalse a op b goto l
Instruction ifFalse(Operand a, const std::string& op, Operand b, const std::string& l) {
    auto i = I(OpCode::IfFalseGoto);
    i.a = a;
    i.op = op;
    i.b = b;
    i.label = l;
    return i;
}
// if a op b goto l   (op vacio: if a goto l)
Instruction ifTrue(Operand a, const std::string& op, Operand b, const std::string& l) {
    auto i = I(OpCode::IfGoto);
    i.a = a;
    i.op = op;
    i.b = b;
    i.label = l;
    return i;
}
Instruction param(Operand a) {
    auto i = I(OpCode::Param);
    i.a = a;
    return i;
}
Instruction call(const std::string& f, int argc, Operand dst = Operand::none()) {
    auto i = I(OpCode::Call);
    i.label = f;
    i.argc = argc;
    i.dst = dst;
    return i;
}
Instruction callInd(Operand addr, int argc, Operand dst) {
    auto i = I(OpCode::Call);
    i.a = addr;
    i.argc = argc;
    i.dst = dst;
    return i;
}
Instruction ret(Operand a = Operand::none()) {
    auto i = I(OpCode::Return);
    i.a = a;
    return i;
}
Instruction load(Operand d, Operand base, Operand off) {  // d = base[off]
    auto i = I(OpCode::Load);
    i.dst = d;
    i.a = base;
    i.b = off;
    return i;
}
Instruction store(Operand base, Operand off, Operand val) {  // base[off] = val
    auto i = I(OpCode::Store);
    i.dst = base;
    i.a = off;
    i.b = val;
    return i;
}
Instruction alloc(Operand d, Operand n) {
    auto i = I(OpCode::Alloc);
    i.dst = d;
    i.a = n;
    return i;
}
Instruction print(const std::string& tipo, Operand a) {
    auto i = I(OpCode::Print);
    i.op = tipo;
    i.a = a;
    return i;
}
Instruction tryBegin(const std::string& l) {
    auto i = I(OpCode::TryBegin);
    i.label = l;
    return i;
}

TacFunction func(const std::string& label, int frame, int temps, std::vector<Instruction> code) {
    TacFunction f;
    f.label = label;
    f.frame_size = frame;
    f.max_temps = temps;
    f.code = std::move(code);
    return f;
}

// Los Symbol tienen que vivir mientras se usa el TacProgram.
std::deque<semantic::Symbol> simbolos;

Operand var(const std::string& name, StorageKind storage, int offset) {
    simbolos.emplace_back();
    semantic::Symbol& s = simbolos.back();
    s.name = name;
    s.ir_name = name;
    s.storage = storage;
    s.offset = offset;
    s.size = 4;
    return Operand::makeVar(&s);
}

// --- Programas ejecutables -------------------------------------------------

struct Ejecutable {
    std::string nombre;
    TacProgram prog;
    std::vector<std::string> salida;
    int exit = 0;
};

std::vector<Ejecutable> ejecutables() {
    std::vector<Ejecutable> out;

    {  // literales enteros, booleanos y un string con comillas y barra
        Ejecutable e{"literales_y_strings", {}, {}, 0};
        e.prog.strings.push_back({"str_0", "comillas \"dobles\" y \\ barra"});
        e.prog.functions.push_back(func("main", 0, 0, {
            print("int", K(42)), print("int", K(-7)), print("bool", K(1)), print("bool", K(0)),
            print("str", S("str_0"))}));
        e.salida = {"42", "-7", "true", "false", "comillas \"dobles\" y \\ barra"};
        out.push_back(std::move(e));
    }
    {  // aritmetica, comparaciones y negaciones
        Ejecutable e{"aritmetica_y_comparaciones", {}, {}, 0};
        e.prog.functions.push_back(func("main", 0, 6, {
            copy(T(0), K(7)), bin("*", T(1), T(0), K(6)), bin("-", T(1), T(1), K(2)),
            bin("/", T(2), T(1), K(4)), bin("%", T(3), T(1), K(4)), un("-", T(4), T(2)),
            print("int", T(1)), print("int", T(2)), print("int", T(3)), print("int", T(4)),
            bin("<", T(5), K(3), K(5)), print("bool", T(5)),
            bin("<=", T(5), K(5), K(4)), print("bool", T(5)),
            bin("==", T(5), K(2), K(2)), print("bool", T(5)),
            bin("!=", T(5), K(2), K(2)), print("bool", T(5)),
            bin(">", T(5), K(7), K(3)), print("bool", T(5)),
            bin(">=", T(5), K(3), K(4)), print("bool", T(5)),
            un("!", T(5), K(0)), print("bool", T(5)),
            un("!", T(5), T(2)), print("bool", T(5))}));
        e.salida = {"40", "10", "0", "-10", "true", "false", "true", "false", "true", "false",
                    "true", "false"};
        out.push_back(std::move(e));
    }
    {  // saltos: bucle con ifFalse/goto, if con comparacion, if sin operador
        Ejecutable e{"saltos", {}, {}, 0};
        e.prog.functions.push_back(func("main", 0, 3, {
            copy(T(0), K(0)), copy(T(1), K(1)),
            lab("S_ini"), ifFalse(T(1), "<=", K(10), "S_fin"),
            bin("+", T(0), T(0), T(1)), bin("+", T(1), T(1), K(1)), go("S_ini"),
            lab("S_fin"), print("int", T(0)),
            ifTrue(T(0), "==", K(55), "S_si"), print("int", K(0)), go("S_fin2"),
            lab("S_si"), print("int", K(1)), lab("S_fin2"),
            copy(T(2), K(0)), ifTrue(T(2), "", NONE(), "S_no"), print("int", K(2)), lab("S_no")}));
        e.salida = {"55", "1", "2"};
        out.push_back(std::move(e));
    }
    {  // frames: 6 parametros, un local, recursion y llamada anidada
        Ejecutable e{"frames_params_recursion", {}, {}, 0};
        std::vector<Operand> p;
        for (int k = 0; k < 6; ++k) p.push_back(var(std::string(1, char('a' + k)), StorageKind::Param, 8 + 4 * k));
        Operand acc = var("acc", StorageKind::Local, -8);
        std::vector<Instruction> suma = {copy(acc, p[0])};
        for (int k = 1; k < 6; ++k) {
            suma.push_back(bin("*", T(0), acc, K(10)));
            suma.push_back(bin("+", acc, T(0), p[k]));
        }
        suma.push_back(ret(acc));
        Operand n = var("n", StorageKind::Param, 8);
        e.prog.functions.push_back(func("main", 0, 2, {
            param(K(6)), param(K(5)), param(K(4)), param(K(3)), param(K(2)), param(K(1)),
            call("fn_suma6", 6, T(0)), print("int", T(0)),
            param(K(6)), call("fn_fact", 1, T(0)), print("int", T(0)),
            param(K(3)), call("fn_fact", 1, T(0)), param(T(0)), call("fn_fact", 1, T(1)),
            print("int", T(1))}));
        e.prog.functions.push_back(func("fn_suma6", 4, 1, suma));
        e.prog.functions.push_back(func("fn_fact", 0, 1, {
            ifFalse(n, ">", K(1), "F_base"), bin("-", T(0), n, K(1)), param(T(0)),
            call("fn_fact", 1, T(0)), bin("*", T(0), n, T(0)), ret(T(0)),
            lab("F_base"), ret(K(1))}));
        e.salida = {"123456", "720", "720"};
        out.push_back(std::move(e));
    }
    {  // desplazamientos que no caben en 16 bits: frame de 40000 bytes y heap
        Ejecutable e{"offsets_fuera_de_16_bits", {}, {}, 0};
        Operand lejos = var("lejos", StorageKind::Local, -(4 + 40000));
        e.prog.functions.push_back(func("main", 0, 2, {
            call("fn_lejos", 0, T(0)), print("int", T(0)),
            alloc(T(0), K(70000)), store(T(0), K(68000), K(5)), load(T(1), T(0), K(68000)),
            print("int", T(1))}));
        e.prog.functions.push_back(func("fn_lejos", 40000, 2, {
            copy(lejos, K(77)), copy(T(1), lejos), bin("+", T(1), T(1), K(1)), ret(T(1))}));
        e.salida = {"78", "5"};
        out.push_back(std::move(e));
    }
    {  // objetos, campos, vtables y llamada indirecta (despacho dinamico)
        Ejecutable e{"objetos_y_vtables", {}, {}, 0};
        e.prog.vtables.push_back({"Animal_vtable", {"Animal_hablar"}});
        e.prog.vtables.push_back({"Perro_vtable", {"Perro_hablar"}});
        e.prog.functions.push_back(func("main", 0, 4, {
            alloc(T(0), K(8)), store(T(0), K(0), L("Perro_vtable")), store(T(0), K(4), K(7)),
            alloc(T(1), K(8)), store(T(1), K(0), L("Animal_vtable")), store(T(1), K(4), K(5)),
            param(T(0)), load(T(2), T(0), K(0)), load(T(2), T(2), K(0)), callInd(T(2), 1, T(2)),
            print("int", T(2)),
            param(T(1)), load(T(2), T(1), K(0)), load(T(2), T(2), K(0)), callInd(T(2), 1, T(2)),
            print("int", T(2)),
            store(T(1), K(4), K(9)), load(T(3), T(1), K(4)), print("int", T(3))}));
        e.prog.functions.push_back(func("Animal_hablar", 0, 1, {
            load(T(0), FP(), K(8)), load(T(0), T(0), K(4)), ret(T(0))}));
        e.prog.functions.push_back(func("Perro_hablar", 0, 1, {
            load(T(0), FP(), K(8)), load(T(0), T(0), K(4)), bin("*", T(0), T(0), K(2)),
            ret(T(0))}));
        e.salida = {"14", "5", "9"};
        out.push_back(std::move(e));
    }
    {  // 40 temporales vivos a la vez que sobreviven a una llamada
        Ejecutable e{"presion_temporales", {}, {}, 0};
        std::vector<Instruction> code;
        for (int k = 0; k < 40; ++k) code.push_back(copy(T(k), K(k + 1)));
        code.push_back(param(K(5)));
        code.push_back(call("fn_id", 1, NONE()));
        for (int k = 1; k < 40; ++k) code.push_back(bin("+", T(0), T(0), T(k)));
        code.push_back(print("int", T(0)));
        e.prog.functions.push_back(func("main", 0, 40, code));
        e.prog.functions.push_back(func("fn_id", 0, 1, {
            load(T(0), FP(), K(8)), ret(T(0))}));
        e.salida = {"820"};
        out.push_back(std::move(e));
    }
    {  // try/catch: __raise salta al catch y __error_message da el mensaje
        Ejecutable e{"try_y_raise", {}, {}, 0};
        e.prog.strings.push_back({"str_0", "boom"});
        auto fin = I(OpCode::TryEnd);
        e.prog.functions.push_back(func("main", 0, 1, {
            tryBegin("R_catch"), param(S("str_0")), call("__raise", 1), print("int", K(0)), fin,
            go("R_fin"), lab("R_catch"), call("__error_message", 0, T(0)), print("str", T(0)),
            lab("R_fin"), print("int", K(1))}));
        e.salida = {"boom", "1"};
        out.push_back(std::move(e));
    }
    {  // referencia nula sin handler: error de runtime y codigo 1
        Ejecutable e{"null_sin_handler", {}, {}, 1};
        e.prog.functions.push_back(func("main", 0, 2, {
            copy(T(0), K(0)), load(T(1), T(0), K(4)), print("int", K(99))}));
        e.salida = {"Error en tiempo de ejecucion: referencia nula"};
        out.push_back(std::move(e));
    }
    return out;
}

// --- Casos de error --------------------------------------------------------

struct Erroneo {
    std::string nombre;
    TacProgram prog;
    const char* codigo;
};

TacProgram conMain(std::vector<Instruction> code, int temps = 2) {
    TacProgram p;
    p.functions.push_back(func("main", 0, temps, std::move(code)));
    return p;
}

std::vector<Erroneo> erroneos() {
    std::vector<Erroneo> out;
    out.push_back({"opcode_desconocido", conMain({I(static_cast<OpCode>(99))}), codes::CG001});
    out.push_back({"operando_ausente", conMain({bin("+", T(0), K(1), NONE())}), codes::CG002});
    out.push_back({"destino_constante", conMain({copy(K(3), K(1))}), codes::CG002});
    out.push_back({"etiqueta_indefinida", conMain({go("L_no_existe")}), codes::CG003});
    out.push_back({"etiqueta_duplicada", conMain({lab("L_x"), lab("L_x")}), codes::CG003});
    {
        TacProgram p = conMain({ret()});
        p.functions.push_back(func("fn_a", 0, 0, {ret()}));
        p.functions.push_back(func("fn_a", 0, 0, {ret()}));
        out.push_back({"funcion_duplicada", p, codes::CG003});
    }
    {
        TacProgram p;
        p.functions.push_back(func("fn_sola", 0, 0, {ret()}));
        out.push_back({"sin_main", p, codes::CG002});
    }
    {
        TacProgram p = conMain({ret()});
        p.functions.push_back(func("fn_neg", -4, 0, {ret()}));
        out.push_back({"frame_negativo", p, codes::CG002});
    }
    {
        TacProgram p = conMain({ret()});
        p.functions.push_back(func("fn_raro", 6, 0, {ret()}));
        out.push_back({"frame_no_multiplo_de_4", p, codes::CG002});
    }
    out.push_back({"temporal_fuera_del_frame", conMain({copy(T(3), K(1))}, 1), codes::CG002});
    {
        TacProgram p = conMain({call("fn_l", 0)});
        p.functions.push_back(func("fn_l", 4, 0, {copy(var("x", StorageKind::Local, -100), K(1)), ret()}));
        out.push_back({"local_fuera_del_frame", p, codes::CG002});
    }
    out.push_back({"offset_no_alineado",
                   conMain({alloc(T(0), K(8)), load(T(1), T(0), K(3))}), codes::CG002});
    {
        TacProgram p = conMain({param(K(1)), call("fn_dos", 2)});
        p.functions.push_back(func("fn_dos", 0, 0, {ret()}));
        out.push_back({"argc_distinto", p, codes::CG002});
    }
    out.push_back({"call_indirecta_sin_direccion", conMain({callInd(NONE(), 0, NONE())}),
                   codes::CG002});
    out.push_back({"alloc_negativo", conMain({alloc(T(0), K(-8))}), codes::CG002});
    out.push_back({"campo_como_variable",
                   conMain({copy(T(0), var("campo", StorageKind::Field, 4))}), codes::CG004});
    return out;
}

bool contiene(const std::vector<diagnostics::Diagnostic>& ds, const char* code) {
    for (const auto& d : ds) {
        if (d.code == code) return true;
    }
    return false;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string emitir;
    if (argc == 3 && std::string(argv[1]) == "--emitir") emitir = argv[2];

    // escapeAsciiz: comillas, barra y salto de linea quedan escapados para .asciiz
    check(mips::escapeAsciiz("a\"b\\c\n") == "a\\\"b\\\\c\\n", "escapeAsciiz",
          "no escapo comillas, barra o salto de linea");

    const std::regex reservados(R"(\$(s[0-7]|k[01]|gp|at)\b)");
    for (auto& e : ejecutables()) {
        mips::MipsResult r = mips::generateMips(e.prog);
        check(r.success, e.nombre, "el generador lo rechazo");
        if (!r.success) {
            for (const auto& d : r.diagnostics) std::cout << "         " << d.code << " " << d.message << "\n";
            continue;
        }
        // Mismo TAC -> mismo ensamblador (etiquetas y strings en orden estable).
        check(mips::generateMips(e.prog).assembly == r.assembly, e.nombre,
              "dos generaciones del mismo TAC dieron .asm distintos");
        // Registros reservados: ni $s (callee-saved), ni $k, $gp o $at.
        check(!std::regex_search(r.assembly, reservados), e.nombre,
              "usa un registro reservado ($s, $k, $gp o $at)");
        // Cada funcion (salvo main) guarda $ra y $fp en su prologo.
        size_t funciones = e.prog.functions.size() - 1;
        size_t guardaRa = 0;
        for (size_t pos = 0; (pos = r.assembly.find("sw $ra, 0($sp)", pos)) != std::string::npos; ++pos) ++guardaRa;
        check(guardaRa >= funciones + 1, e.nombre, "alguna funcion no guarda $ra");
        if (e.nombre == "objetos_y_vtables") {
            check(r.assembly.find("jalr $t3") != std::string::npos, e.nombre,
                  "la llamada indirecta no usa jalr");
        }
        if (e.nombre == "offsets_fuera_de_16_bits") {
            check(r.assembly.find("li $t9, -40") != std::string::npos &&
                      r.assembly.find("li $t9, 68000") != std::string::npos,
                  e.nombre, "no sintetiza en $t9 las direcciones fuera de 16 bits");
        }
        if (!emitir.empty()) {
            std::ofstream(emitir + "/" + e.nombre + ".asm") << r.assembly;
            std::ofstream out(emitir + "/" + e.nombre + ".out");
            for (const auto& linea : e.salida) out << linea << "\n";
            std::ofstream(emitir + "/" + e.nombre + ".exit") << e.exit << "\n";
        }
    }

    for (auto& e : erroneos()) {
        mips::MipsResult r = mips::generateMips(e.prog);
        check(!r.success, e.nombre, "debia rechazarse");
        check(r.assembly.empty(), e.nombre, "dejo ensamblador (parcial) aunque fallo");
        check(contiene(r.diagnostics, e.codigo), e.nombre,
              std::string("no reporto ") + e.codigo);
        for (const auto& d : r.diagnostics) {
            check(!d.message.empty(), e.nombre, "diagnostico sin mensaje");
        }
    }

    std::cout << total - fallos << "/" << total << " verificaciones ok";
    if (!emitir.empty()) std::cout << " (ejecutables escritos en " << emitir << ")";
    std::cout << "\n";
    return fallos == 0 ? 0 : 1;
}
