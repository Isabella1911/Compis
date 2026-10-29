#ifndef SQLINT_DIAGNOSTICS_CODES_H
#define SQLINT_DIAGNOSTICS_CODES_H

// Registro de codigos de diagnostico (tabla A.8 de docs/sql). Las pruebas
// comparan codigos, no mensajes: cambiar la redaccion de un mensaje no debe
// romper ninguna prueba, cambiar un codigo si.
//
// Dueno: Parte 1. Las Partes 2 y 3 piden sus codigos y se agregan aqui;
// los rangos RUN1xx (Parte 2) y RUN2xx (Parte 3) ya estan reservados.

namespace sqlint {
namespace diagnostics {
namespace codes {

// Lexico
constexpr const char* LEX001 = "LEX001";  // Caracter no reconocido
constexpr const char* LEX002 = "LEX002";  // Cadena sin cerrar

// Sintactico
constexpr const char* SYN001 = "SYN001";  // Error de sintaxis (mensaje de ANTLR traducido)

// Semantico
constexpr const char* SEM001 = "SEM001";  // Tabla inexistente
constexpr const char* SEM002 = "SEM002";  // Columna inexistente
constexpr const char* SEM003 = "SEM003";  // Referencia ambigua
constexpr const char* SEM004 = "SEM004";  // Calificador invalido (x.col con x fuera del FROM)
constexpr const char* SEM005 = "SEM005";  // Alias o tabla repetida en el FROM
constexpr const char* SEM006 = "SEM006";  // Tipos incompatibles en comparacion
constexpr const char* SEM007 = "SEM007";  // Tipos incompatibles en aritmetica o logica
constexpr const char* SEM008 = "SEM008";  // INSERT: cantidad de valores != columnas
constexpr const char* SEM009 = "SEM009";  // INSERT/UPDATE: valor incompatible con la columna
constexpr const char* SEM010 = "SEM010";  // Literal VARCHAR mas largo que n
constexpr const char* SEM011 = "SEM011";  // Fecha invalida
constexpr const char* SEM012 = "SEM012";  // NULL en columna NOT NULL
constexpr const char* SEM013 = "SEM013";  // CREATE TABLE de una tabla existente
constexpr const char* SEM014 = "SEM014";  // CREATE TABLE mal formado
constexpr const char* SEM015 = "SEM015";  // Columna repetida en INSERT o SET
constexpr const char* SEM016 = "SEM016";  // Indice duplicado
constexpr const char* SEM017 = "SEM017";  // Indice inexistente
constexpr const char* SEM018 = "SEM018";  // Subconsulta escalar con mas de una columna
constexpr const char* SEM019 = "SEM019";  // Subconsulta de IN con mas de una columna
constexpr const char* SEM020 = "SEM020";  // Agregado en WHERE o agregados anidados
constexpr const char* SEM021 = "SEM021";  // Columna no agrupada con GROUP BY/agregados
constexpr const char* SEM022 = "SEM022";  // SUM/AVG sobre tipo no numerico
constexpr const char* SEM023 = "SEM023";  // Construccion aun no soportada en esta entrega

// Ejecucion, Parte 2
constexpr const char* RUN101 = "RUN101";  // Llave primaria duplicada
constexpr const char* RUN102 = "RUN102";  // NOT NULL violado en UPDATE
constexpr const char* RUN103 = "RUN103";  // Archivo de la base faltante o corrupto
constexpr const char* RUN104 = "RUN104";  // Valor calculado excede VARCHAR(n)

// Ejecucion, Parte 3
constexpr const char* RUN201 = "RUN201";  // Subconsulta escalar devolvio mas de una fila
constexpr const char* RUN202 = "RUN202";  // Division entre cero

}  // namespace codes
}  // namespace diagnostics
}  // namespace sqlint

#endif  // SQLINT_DIAGNOSTICS_CODES_H
