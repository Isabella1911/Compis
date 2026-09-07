#ifndef COMPISCRIPT_DIAGNOSTICS_CODES_H
#define COMPISCRIPT_DIAGNOSTICS_CODES_H

// Registro de codigos de diagnostico. Un codigo por linea, para que los
// tests comparen codigos ("SEM011") y no mensajes en texto libre (fragiles
// ante cualquier cambio de redaccion). Prefijos: LEX0xx, SYN0xx, SEM0xx.

namespace compiscript {
namespace diagnostics {
namespace codes {

constexpr const char* SYN001 = "SYN001";  // Error sintactico generico (delegado por ANTLR)

// Diagnosticos semanticos emitidos por los pases.
constexpr const char* SEM001 = "SEM001";  // Variable no declarada
constexpr const char* SEM002 = "SEM002";  // Redeclaracion en el mismo ambito
constexpr const char* SEM003 = "SEM003";  // Tipo incompatible o destino no asignable/inmutable
constexpr const char* SEM004 = "SEM004";  // Tipo incompatible en operacion aritmetica/logica
constexpr const char* SEM005 = "SEM005";  // Condicion no booleana (if/while/do-while/for/ternario)
constexpr const char* SEM006 = "SEM006";  // break/continue fuera de un bucle
constexpr const char* SEM007 = "SEM007";  // return fuera de una funcion
constexpr const char* SEM008 = "SEM008";  // Numero o tipo de argumentos incorrecto en llamada
constexpr const char* SEM009 = "SEM009";  // Tipo de retorno incompatible
constexpr const char* SEM010 = "SEM010";  // Acceso a atributo/metodo inexistente
constexpr const char* SEM011 = "SEM011";  // Parametro de funcion sin anotacion de tipo (exigido por decision de lenguaje)
constexpr const char* SEM012 = "SEM012";  // Codigo muerto (instrucciones tras return/break/continue)
constexpr const char* SEM013 = "SEM013";  // Nombre de tipo invalido (ni primitivo ni clase declarada)
constexpr const char* SEM014 = "SEM014";  // Herencia circular entre clases
constexpr const char* SEM015 = "SEM015";  // 'this' usado fuera del ambito de clase

constexpr const char* SEM016 = "SEM016";  // Valor no invocable
constexpr const char* SEM017 = "SEM017";  // new sobre no-clase o constructor no-metodo
constexpr const char* SEM018 = "SEM018";  // foreach sobre no-arreglo
constexpr const char* SEM019 = "SEM019";  // Tipo no inferible o valor void usado como dato
constexpr const char* SEM020 = "SEM020";  // Funcion con retorno puede terminar sin devolver valor

}  // namespace codes
}  // namespace diagnostics
}  // namespace compiscript

#endif  // COMPISCRIPT_DIAGNOSTICS_CODES_H
