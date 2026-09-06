#ifndef COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H
#define COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H

// Pass 3 del analisis semantico: recorre el AST llenando
// AstNode::resolved_type (de abajo hacia arriba: el tipo de un nodo se
// calcula a partir de sus hijos, como explican los apuntes de clase sobre
// atributos sintetizados) y valida las reglas de "Sistema de Tipos" y
// parte de "Control de Flujo" del PDF.
//
// Cubre: tipos en operaciones aritmeticas/logicas/comparaciones (SEM004),
// tipos en asignaciones (SEM003), condiciones booleanas de
// if/while/do-while/for/ternario (SEM005), tipo de retorno (SEM009),
// compatibilidad de tipos en switch/case (SEM004), tipos de elementos de
// arreglo e indices (SEM004), y nombres de tipo invalidos en anotaciones
// (SEM013).
//
// Tambien cubre, ahora que InheritanceResolver ya resuelve la cadena de
// herencia: acceso a atributos/metodos existentes (SEM010, buscando
// primero en la clase y despues subiendo por sus bases), 'this' tipado
// como la clase contenedora y fuera de contexto (SEM015), numero/tipo de
// argumentos en llamadas a funciones y metodos, y en el constructor de
// 'new' (SEM008).
//
// Deliberadamente NO cubre todavia (ver docs/03_passes_semanticos.md):
//   - break/continue fuera de bucle (SEM006) y codigo muerto (SEM012): son
//     control de flujo puro, no necesitan tipos -- pass aparte.
//   - analisis de closures (que variables captura cada funcion anidada):
//     no es una validacion, es informacion para generacion de codigo.
//
// Limitacion conocida de orden: si una funcion o metodo se USA antes de
// que este mismo pass haya procesado su propia declaracion (p. ej. una
// clase declarada mas abajo en el archivo cuyo metodo se llama antes),
// FunctionSymbol::resolved_type todavia no existe en ese punto -- el
// resultado es que esa llamada puntual queda sin validar (Type::Error
// silencioso, no un diagnostico incorrecto). No afecta la resolucion de
// NOMBRES (eso ya es order-independent via NameResolver/InheritanceResolver),
// solo la validacion de tipos/argumentos de ESA llamada especifica.

#include "ast/nodes.h"
#include "diagnostics/reporter.h"
#include "scope.h"
#include "type.h"

namespace compiscript {
namespace semantic {

class TypeChecker {
public:
    explicit TypeChecker(diagnostics::DiagnosticReporter& reporter);

    void run(ast::Program& program);

private:
    void checkStatement(ast::Statement* stmt);
    TypePtr checkExpression(ast::Expression* expr, Scope* scope);

    // Recorre una anotacion de tipo (declarada, o null) y la resuelve.
    TypePtr resolveDeclaredType(const ast::TypeAnnotationPtr& annotation, Scope* scope);

    // Chequea que `actual` sea compatible con `expected`; si no, reporta
    // `code` en (line, column). Retorna si eran compatibles.
    bool checkCompatible(const TypePtr& expected, const TypePtr& actual, const char* code,
                          const std::string& message, int line, int column);

    Scope* findEnclosingFunctionScope(Scope* scope) const;
    Scope* findEnclosingClassScope(Scope* scope) const;

    // Busca `name` en `cls` y, si no esta, sube por su cadena de
    // herencia (base_class). No propietario (vive en algun class_scope).
    Symbol* lookupMember(ClassSymbol* cls, const std::string& name) const;

    // Resuelve el acceso `objectExpr.memberName`: chequea el objeto,
    // valida que sea una clase, busca el miembro (subiendo por la
    // herencia). Retorna nullptr si ya se reporto un error (o si el
    // objeto todavia no tiene tipo resuelto, para no reportar en cascada).
    Symbol* resolveMemberAccess(ast::Expression* objectExpr, const std::string& memberName,
                                 Scope* scope, int line, int column);

    // Compara argTypes contra paramTypes (numero y tipo, posicional);
    // reporta SEM008 si no coinciden.
    void checkArguments(const std::vector<TypePtr>& paramTypes,
                         const std::vector<TypePtr>& argTypes, int line, int column);

    diagnostics::DiagnosticReporter& reporter_;
};

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_TYPE_CHECKER_H
