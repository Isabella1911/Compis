#ifndef COMPISCRIPT_IR_TAC_H
#define COMPISCRIPT_IR_TAC_H

// Representacion en memoria del codigo de tres direcciones (TAC). Es la
// salida de codegen/TacGenerator y la entrada de la fase siguiente (MIPS).
// El formato de texto lo define ir/tac_printer.cpp y esta documentado en
// docs/08_lenguaje_intermedio.md.
//
// Convenciones que heredan todas las fases (pensadas para que el paso a
// MIPS sea casi linea por linea):
//   - Todo valor ocupa 4 bytes: integer, boolean (0/1) y punteros
//     (string, listas, objetos). null es 0.
//   - Los offsets de Load/Store/Alloc estan en BYTES.
//   - Las etiquetas son identificadores validos en MIPS.

#include <string>
#include <vector>

namespace compiscript {

namespace semantic {
class Symbol;
class FunctionSymbol;
}  // namespace semantic

namespace ir {

enum class OperandKind {
    None,      // operando ausente (p. ej. 'return' sin valor)
    Temp,      // t0, t1, ... (reciclables, ver codegen/temp_allocator.h)
    Var,       // variable del programa fuente (global, local o parametro)
    IntConst,  // constante entera; tambien true=1, false=0, null=0
    String,    // etiqueta de un literal de texto (str_0, str_1, ...)
    FramePtr,  // fp: base del registro de activacion actual
    Label,     // direccion de una etiqueta de datos (p. ej. Perro_vtable)
};

struct Operand {
    OperandKind kind = OperandKind::None;
    int temp = -1;                         // solo Temp
    int value = 0;                         // solo IntConst
    semantic::Symbol* symbol = nullptr;    // solo Var (no propietario)
    std::string name;                      // Var: nombre en el TAC; String: etiqueta

    static Operand none() { return {}; }
    static Operand makeTemp(int index);
    static Operand makeInt(int value);
    static Operand makeString(const std::string& label);
    static Operand makeFramePtr();
    static Operand makeLabel(const std::string& label);
    // El nombre que se imprime sale de Symbol::ir_name (lo fija FrameLayout);
    // si todavia esta vacio, se usa el nombre del codigo fuente.
    static Operand makeVar(semantic::Symbol* symbol);

    bool isNone() const { return kind == OperandKind::None; }
    bool isTemp() const { return kind == OperandKind::Temp; }
    bool isConst() const { return kind == OperandKind::IntConst; }
};

enum class OpCode {
    Copy,         // dst = a
    Binary,       // dst = a op b          op: + - * / % == != < <= > >=
    Unary,        // dst = op a            op: - !
    Label,        // label:
    Goto,         // goto label
    IfGoto,       // if a goto label       | if a op b goto label
    IfFalseGoto,  // ifFalse a goto label  | ifFalse a op b goto label
    Param,        // param a
    Link,         // link a                (enlace de acceso para el proximo call)
    Call,         // [dst =] call label, argc   |  [dst =] call a, argc (indirecta: a = direccion)
    Return,       // return [a]
    Load,         // dst = a[b]            (palabra en la direccion a + b)
    Store,        // dst[a] = b            (guarda b en la direccion dst + a)
    Alloc,        // dst = alloc a         (a bytes en el heap)
    Print,        // print_<op> a          op: int | str | bool
    TryBegin,     // try_begin label       (label = inicio del catch)
    TryEnd,       // try_end
    Comment,      // # text
};

struct Instruction {
    OpCode code;
    std::string op;     // operador (Binary/Unary/IfGoto/IfFalseGoto) o tipo (Print)
    Operand dst;
    Operand a;
    Operand b;
    std::string label;  // Label/Goto/IfGoto/IfFalseGoto/Call/TryBegin (Call indirecta: vacio, usa a)
    int argc = 0;       // solo Call
    std::string text;   // solo Comment
};

// Un bloque `func ... endfunc`. El codigo de nivel superior del programa
// es la funcion `main` (symbol == nullptr).
struct TacFunction {
    std::string label;
    semantic::FunctionSymbol* symbol = nullptr;  // null para main
    int frame_size = 0;   // bytes de locales (sin contar temporales)
    int max_temps = 0;    // cuantos temporales distintos usa a la vez, como maximo
    std::vector<Instruction> code;
};

struct StringLiteral {
    std::string label;  // str_<n>
    std::string value;  // texto ya sin comillas (las secuencias de escape se dejan tal cual)
};

// Tabla de metodos de una clase: la palabra 0 de cada objeto apunta a su
// vtable, y la palabra i es la direccion del metodo del slot i.
struct VTable {
    std::string label;                 // <Clase>_vtable
    std::vector<std::string> methods;  // etiquetas de los metodos, por slot
};

struct GlobalVar {
    std::string label;  // g_<nombre>
    int size = 4;
};

struct TacProgram {
    std::vector<GlobalVar> globals;
    std::vector<StringLiteral> strings;
    std::vector<VTable> vtables;  // una por clase, en orden de declaracion
    std::vector<TacFunction> functions;  // main primero, luego en orden de declaracion
};

}  // namespace ir
}  // namespace compiscript

#endif  // COMPISCRIPT_IR_TAC_H
