#ifndef COMPISCRIPT_DIAGNOSTICS_CODES_H
#define COMPISCRIPT_DIAGNOSTICS_CODES_H

// Registro de codigos de diagnostico. Un codigo por linea, para que los
// tests comparen codigos ("SEM011") y no mensajes en texto libre (fragiles
// ante cualquier cambio de redaccion). Prefijos: LEX0xx, SYN0xx, SEM0xx.
//
// Esta etapa (Fundacion) solo produce SYN001, generado por el
// DiagnosticErrorListener cuando ANTLR reporta un error sintactico. Los
// codigos SEM0xx los agrega la Etapa 3 (analisis semantico); se listan
// aqui por adelantado porque ya estan decididos en el README de decisiones
// de lenguaje y no van a cambiar de numero despues.

namespace compiscript {
namespace diagnostics {
namespace codes {

constexpr const char* SYN001 = "SYN001";  // Error sintactico generico (delegado por ANTLR)

// Reservados para la Etapa 3 (analisis semantico). No se emiten todavia.
constexpr const char* SEM001 = "SEM001";  // Variable no declarada
constexpr const char* SEM002 = "SEM002";  // Redeclaracion en el mismo ambito
constexpr const char* SEM003 = "SEM003";  // Tipo incompatible en asignacion
constexpr const char* SEM004 = "SEM004";  // Tipo incompatible en operacion aritmetica/logica
constexpr const char* SEM005 = "SEM005";  // Condicion no booleana (if/while/do-while/for/switch)
constexpr const char* SEM006 = "SEM006";  // break/continue fuera de un bucle
constexpr const char* SEM007 = "SEM007";  // return fuera de una funcion
constexpr const char* SEM008 = "SEM008";  // Numero o tipo de argumentos incorrecto en llamada
constexpr const char* SEM009 = "SEM009";  // Tipo de retorno incompatible
constexpr const char* SEM010 = "SEM010";  // Acceso a atributo/metodo inexistente
constexpr const char* SEM011 = "SEM011";  // Parametro de funcion sin anotacion de tipo (exigido por decision de lenguaje)
constexpr const char* SEM012 = "SEM012";  // Codigo muerto (instrucciones tras return/break/continue)

}  // namespace codes
}  // namespace diagnostics
}  // namespace compiscript

#endif  // COMPISCRIPT_DIAGNOSTICS_CODES_H
