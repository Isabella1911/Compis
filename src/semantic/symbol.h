#ifndef COMPISCRIPT_SEMANTIC_SYMBOL_H
#define COMPISCRIPT_SEMANTIC_SYMBOL_H

// Simbolo de la tabla de simbolos. Un Symbol por cada variable, constante,
// parametro, funcion o clase declarada.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ast/nodes.h"
#include "type.h"

namespace compiscript {
namespace semantic {

class Scope;           // adelante, definido en scope.h
class FunctionSymbol;  // adelante

enum class SymbolKind { Variable, Constant, Parameter, Function, Class };

// Donde vive el valor en tiempo de ejecucion. Lo fija codegen/FrameLayout
// (Global/Param/Local) o el layout de clases de la parte 2 (Field).
enum class StorageKind { None, Global, Param, Local, Field };

class Symbol {
public:
    std::string name;
    SymbolKind kind;
    ast::TypeAnnotationPtr declared_type;  // tal como se escribio; puede ser null si no se anoto
    TypePtr resolved_type;                 // llenado por TypeChecker; null hasta entonces
    int declared_line = 0;
    int declared_column = 0;
    bool is_mutable = true;  // false para 'const'

    // --- Informacion para generacion de codigo (Proyecto 2, TAC) ---
    StorageKind storage = StorageKind::None;
    // Param/Local: bytes relativos a fp (ver registro de activacion en
    // docs/08). Field: bytes desde el inicio del objeto. Global: no aplica.
    int offset = 0;
    int size = 0;          // siempre 4 por ahora: todo valor es una palabra
    // Nombre con el que aparece en el TAC: g_<nombre> para globales; para
    // locales el nombre fuente, con sufijo @n si otro simbolo de la misma
    // funcion ya lo usa (sombreado).
    std::string ir_name;
    // Funcion que declara este local/parametro (null en globales y campos).
    // Lo fija FrameLayout; lo usan genCapturedLoad/Store para saber cuantos
    // enlaces de acceso subir.
    FunctionSymbol* owner_function = nullptr;

    virtual ~Symbol() = default;
};
using SymbolPtr = std::shared_ptr<Symbol>;

// Una funcion necesita ademas su lista de parametros, tipo de retorno, y
// (cuando exista su cuerpo) el Scope propio donde viven esos parametros.
class FunctionSymbol : public Symbol {
public:
    std::vector<ast::Parameter> params;
    ast::TypeAnnotationPtr return_type;  // null -> se asume void
    Scope* function_scope = nullptr;     // no propietario; lo crea quien arma la tabla

    // Variables resueltas fuera de esta funcion pero usadas dentro de su
    // cuerpo (closures). Lo llena ClosureAnalyzer; no valida nada, es
    // informacion para cuando se generen closures reales en tiempo de
    // ejecucion (fuera de este proyecto). No propietario, sin duplicados.
    std::vector<Symbol*> captured;

    // --- Generacion de codigo ---
    std::string label;      // fn_<nombre>, fn_<externa>_<interna>, <Clase>_<metodo>
    int frame_size = 0;     // bytes de locales en su registro de activacion
    int nesting_depth = 0;  // 0 = declarada en el nivel superior (o metodo de clase)
    bool is_method = false; // metodo o constructor: 'this' es el parametro 0
};
using FunctionSymbolPtr = std::shared_ptr<FunctionSymbol>;

// Una clase necesita el nombre de su base (herencia con ':') y el Scope
// donde viven sus atributos/metodos.
class ClassSymbol : public Symbol {
public:
    std::optional<std::string> base_class_name;  // tal como se escribio (':' Identifier)
    ClassSymbol* base_class = nullptr;  // resuelto por InheritanceResolver; null hasta entonces
                                         // (y null para siempre si no hay ':' o si no resolvio)
    Scope* class_scope = nullptr;       // no propietario

    // --- Generacion de codigo (lo llena la parte 2) ---
    int instance_size = 0;  // bytes de un objeto, atributos heredados incluidos
};
using ClassSymbolPtr = std::shared_ptr<ClassSymbol>;

}  // namespace semantic
}  // namespace compiscript

#endif  // COMPISCRIPT_SEMANTIC_SYMBOL_H
