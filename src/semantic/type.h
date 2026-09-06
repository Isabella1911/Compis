#ifndef COMPISCRIPT_SEMANTIC_TYPE_H
#define COMPISCRIPT_SEMANTIC_TYPE_H

// Sistema de tipos interno. Un Type es lo que resulta de resolver un
// ast::TypeAnnotation (o de inferirlo de una expresion) contra la tabla de
// simbolos -- a diferencia de TypeAnnotation, que es solo "lo que el
// programador escribio", un Type ya sabe, por ejemplo, a que ClassSymbol
// se refiere un nombre de clase.
//
// Ver docs/02_sistema_de_tipos.md para el diseño completo y las
// decisiones pendientes.

#include <memory>
#include <string>
#include <vector>

#include "ast/nodes.h"
#include "diagnostics/reporter.h"

namespace compiscript {
namespace semantic {

class ClassSymbol;  // adelante, definido en symbol.h
class Scope;        // adelante, definido en scope.h

enum class TypeKind { Integer, String, Boolean, Null, Void, Array, Function, Class, Error };

// TypeKind::Error es un comodin: representa "no se pudo determinar el
// tipo" (porque ya hubo un error antes). Comparado contra CUALQUIER otro
// tipo siempre da compatible -- asi un error no dispara una cascada de
// errores secundarios por cada operacion que use ese valor despues.
class Type {
public:
    TypeKind kind;

    // Solo aplica si kind == Array:
    std::shared_ptr<Type> element_type;
    // Solo aplica si kind == Class:
    ClassSymbol* class_symbol = nullptr;
    // Solo aplica si kind == Function:
    std::vector<std::shared_ptr<Type>> param_types;
    std::shared_ptr<Type> return_type;

    // Comparacion estructural. No es igualdad de puntero: dos
    // ArrayTypeAnnotation("integer") distintos en el codigo fuente deben
    // dar Type equals() == true.
    bool equals(const Type& other) const;
};
using TypePtr = std::shared_ptr<Type>;

TypePtr makeIntegerType();
TypePtr makeStringType();
TypePtr makeBooleanType();
TypePtr makeNullType();
TypePtr makeVoidType();
TypePtr makeErrorType();
TypePtr makeArrayType(TypePtr element);
TypePtr makeClassType(ClassSymbol* classSymbol);
TypePtr makeFunctionType(std::vector<TypePtr> params, TypePtr returnType);

// Convierte un ast::TypeAnnotation (lo que el programador escribio) en un
// Type resuelto. Para un nombre de clase, resuelve contra `scope` y
// reporta SEM013 si el nombre no existe o no es una clase.
TypePtr resolveTypeAnnotation(ast::TypeAnnotation* annotation, Scope* scope,
                               diagnostics::DiagnosticReporter& reporter);

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_TYPE_H
