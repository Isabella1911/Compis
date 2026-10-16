#ifndef COMPISCRIPT_CODEGEN_TAC_GENERATOR_H
#define COMPISCRIPT_CODEGEN_TAC_GENERATOR_H

// Traduce el AST (ya validado por los pases semanticos, sin errores) a TAC.
// Es un recorrido dirigido por la sintaxis: cada metodo gen* es la accion
// semantica de un tipo de nodo.
//
// La implementacion esta repartida por construccion (y por integrante):
//   tac_generator.cpp    infraestructura: contexto de funcion, emit*, temporales,
//                        etiquetas, strings, despacho de nodos        (parte 1)
//   gen_expressions.cpp  expresiones, condiciones con saltos         (parte 1)
//   gen_statements.cpp   declaraciones, print, control de flujo      (parte 1)
//   gen_functions.cpp    funciones, llamadas, return, closures       (parte 2)
//   gen_classes.cpp      clases, new, atributos, metodos, this       (parte 2)
//   gen_arrays.cpp       listas, indices, foreach                    (parte 2)
//
// Convencion de operandos (vale para TODOS los gen* que devuelven
// ir::Operand): el operando devuelto puede ser un temporal, una variable o
// una constante. Quien lo recibe y lo consume (lo usa como operando de una
// instruccion) debe llamar release() sobre el despues; release() ignora lo
// que no sea temporal. Asi los temporales se reciclan sin que nadie tenga
// que preguntar de donde vino el valor.

#include <string>
#include <vector>

#include "ast/nodes.h"
#include "ir/tac.h"
#include "semantic/symbol.h"
#include "temp_allocator.h"

namespace compiscript {
namespace codegen {

class TacGenerator {
public:
    ir::TacProgram generate(ast::Program& program, int mainFrameSize);

private:
    // ---------------------------------------------------------------
    // Infraestructura (tac_generator.cpp)
    // ---------------------------------------------------------------

    struct LoopLabels {
        std::string break_label;
        std::string continue_label;
    };

    // Estado de la funcion que se esta generando. Es una pila porque una
    // funcion anidada (o un metodo) se genera en medio de la que la
    // contiene: beginFunction apila y endFunction desapila y guarda.
    struct FunctionContext {
        ir::TacFunction fn;
        TempAllocator temps;
        std::vector<LoopLabels> loops;
    };

    // Abre un bloque `func`. Lo que se emita hasta endFunction() va dentro.
    void beginFunction(const std::string& label, semantic::FunctionSymbol* symbol,
                       int frameSize);
    void endFunction();

    void emit(ir::Instruction instruction);
    void emitCopy(const ir::Operand& dst, const ir::Operand& src);
    void emitBinary(const ir::Operand& dst, const std::string& op, const ir::Operand& a,
                    const ir::Operand& b);
    void emitUnary(const ir::Operand& dst, const std::string& op, const ir::Operand& a);
    void emitLabel(const std::string& label);
    void emitGoto(const std::string& label);
    void emitIf(const ir::Operand& cond, const std::string& label);       // if a goto L
    void emitIfFalse(const ir::Operand& cond, const std::string& label);  // ifFalse a goto L
    void emitIfRel(const ir::Operand& a, const std::string& op, const ir::Operand& b,
                   const std::string& label);                             // if a op b goto L
    void emitIfFalseRel(const ir::Operand& a, const std::string& op, const ir::Operand& b,
                        const std::string& label);                        // ifFalse a op b goto L
    void emitParam(const ir::Operand& a);
    void emitLink(const ir::Operand& a);
    // Con hasResult devuelve un temporal nuevo con el valor de retorno;
    // sin el, devuelve Operand::none().
    ir::Operand emitCall(const std::string& label, int argc, bool hasResult);
    void emitReturn(const ir::Operand& value = ir::Operand::none());
    ir::Operand emitLoad(const ir::Operand& base, const ir::Operand& offset);  // t = base[offset]
    void emitStore(const ir::Operand& base, const ir::Operand& offset,
                   const ir::Operand& value);                                 // base[offset] = value
    ir::Operand emitAlloc(const ir::Operand& bytes);                          // t = alloc bytes
    void emitPrint(const std::string& kind, const ir::Operand& a);            // kind: int|str|bool
    void emitComment(const std::string& text);

    ir::Operand newTemp();
    void release(const ir::Operand& operand);
    std::string newLabel();  // L0, L1, ... (unicas en todo el programa)
    // Registra un literal de texto (sin comillas) y devuelve su operando
    // (str_<n>). Literales iguales comparten etiqueta.
    ir::Operand internString(const std::string& value);
    ir::Operand varOperand(semantic::Symbol* symbol);

    // Pila de bucles de la funcion actual, para break/continue (y foreach).
    void pushLoop(const std::string& breakLabel, const std::string& continueLabel);
    void popLoop();
    const LoopLabels* currentLoop() const;  // null si no hay bucle abierto

    // Marca una construccion que todavia no se traduce: emite un
    // comentario `# TODO(<owner>): <what>` y devuelve la constante 0 para
    // que el resto de la traduccion pueda seguir.
    ir::Operand todo(const std::string& owner, const std::string& what);

    FunctionContext& current() { return contexts_.back(); }

    // Despacho por tipo de nodo (tac_generator.cpp).
    void genStatement(ast::Statement* stmt);
    void genStatements(const std::vector<ast::StatementPtr>& statements);
    ir::Operand genExpr(ast::Expression* expr);

    // ---------------------------------------------------------------
    // Parte 1: expresiones (gen_expressions.cpp)
    // ---------------------------------------------------------------
    ir::Operand genLiteral(ast::LiteralExpression* n);
    ir::Operand genIdentifier(ast::IdentifierExpression* n);
    ir::Operand genBinary(ast::BinaryExpression* n);
    ir::Operand genUnary(ast::UnaryExpression* n);
    ir::Operand genAssignment(ast::AssignmentExpression* n);
    ir::Operand genTernary(ast::TernaryExpression* n);
    // Codigo con saltos para una condicion booleana: salta a trueLabel si
    // es verdadera y a falseLabel si es falsa. Una etiqueta vacia significa
    // "seguir de largo" (fall through) en ese caso. && y || se traducen
    // aqui con cortocircuito.
    void genCond(ast::Expression* expr, const std::string& trueLabel,
                 const std::string& falseLabel);

    // ---------------------------------------------------------------
    // Parte 1: sentencias (gen_statements.cpp)
    // ---------------------------------------------------------------
    void genBlock(ast::Block* n);
    void genVariableDeclaration(ast::VariableDeclaration* n);
    void genConstantDeclaration(ast::ConstantDeclaration* n);
    void genAssignmentStatement(ast::AssignmentStatement* n);
    void genExpressionStatement(ast::ExpressionStatement* n);
    void genPrint(ast::PrintStatement* n);
    void genIf(ast::IfStatement* n);
    void genWhile(ast::WhileStatement* n);
    void genDoWhile(ast::DoWhileStatement* n);
    void genFor(ast::ForStatement* n);
    void genSwitch(ast::SwitchStatement* n);
    void genTryCatch(ast::TryCatchStatement* n);
    void genBreak(ast::BreakStatement* n);
    void genContinue(ast::ContinueStatement* n);

    // ---------------------------------------------------------------
    // Parte 2: funciones y closures (gen_functions.cpp)
    // ---------------------------------------------------------------
    void genFunctionDeclaration(ast::FunctionDeclaration* n);
    void genReturn(ast::ReturnStatement* n);
    ir::Operand genCall(ast::CallExpression* n);

    // ---------------------------------------------------------------
    // Parte 2: clases y objetos (gen_classes.cpp)
    // ---------------------------------------------------------------
    void genClassDeclaration(ast::ClassDeclaration* n);
    ir::Operand genNew(ast::NewExpression* n);
    ir::Operand genMemberAccess(ast::MemberAccessExpression* n);
    // `object.member = value`, tanto la sentencia (PropertyAssignment) como
    // la expresion (PropertyAssignExpr). `member` es el simbolo del
    // atributo que ya resolvio TypeChecker (node->symbol). Devuelve el
    // valor asignado.
    ir::Operand genPropertyAssignment(ast::Expression* object, semantic::Symbol* member,
                                      ast::Expression* value);
    ir::Operand genThis(ast::ThisExpression* n);

    // ---------------------------------------------------------------
    // Parte 2: listas (gen_arrays.cpp)
    // ---------------------------------------------------------------
    ir::Operand genArrayLiteral(ast::ArrayLiteral* n);
    ir::Operand genArrayAccess(ast::ArrayAccessExpression* n);
    // `target = value` cuando target es `lista[indice]`. Devuelve el valor asignado.
    ir::Operand genIndexAssignment(ast::ArrayAccessExpression* target, ast::Expression* value);
    void genForeach(ast::ForeachStatement* n);

    // ---------------------------------------------------------------
    // Estado
    // ---------------------------------------------------------------
    ir::TacProgram program_;
    std::vector<FunctionContext> contexts_;
    int next_label_ = 0;
};

}  // namespace codegen
}  // namespace compiscript

#endif  // COMPISCRIPT_CODEGEN_TAC_GENERATOR_H
