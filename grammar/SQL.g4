// Gramatica del interprete SQL (Parte 1).
//
// Cubre todo el "SQL Requerido" del enunciado, incluidas las construcciones
// de la entrega 2 (JOIN, agregados, GROUP BY/HAVING, subconsultas e indices):
// rehacer la gramatica a mitad de camino romperia el AST y los visitors de
// las tres partes. El semantico decide que esta soportado en cada entrega.
//
// Palabras clave sin distincion de mayusculas (caseInsensitive, ANTLR >= 4.10):
// SELECT, select y Select son el mismo token. Los identificadores conservan
// el texto original; la comparacion sin mayusculas la hace el catalogo.

grammar SQL;

options { caseInsensitive = true; }

// ---------------------------------------------------------------------------
// Script
// ---------------------------------------------------------------------------

// Sentencias separadas por ';'. El ';' final es opcional y se toleran ';'
// repetidos (sentencias vacias).
script
    : ';'* (statement (';'+ statement)* ';'*)? EOF
    ;

statement
    : createTableStmt
    | dropTableStmt
    | createIndexStmt
    | dropIndexStmt
    | insertStmt
    | updateStmt
    | deleteStmt
    | selectStmt
    ;

// ---------------------------------------------------------------------------
// DDL
// ---------------------------------------------------------------------------

createTableStmt
    : CREATE TABLE identifier '(' tableElement (',' tableElement)* ')'
    ;

tableElement
    : identifier dataType columnConstraint*        # columnElement
    | PRIMARY KEY '(' identifier ')'               # primaryKeyElement
    ;

columnConstraint
    : NOT NULL                                     # notNullConstraint
    | PRIMARY KEY                                  # primaryKeyConstraint
    ;

dataType
    : INT                                          # intType
    | FLOAT                                        # floatType
    | VARCHAR '(' INTEGER_LITERAL ')'              # varcharType
    | BOOLEAN                                      # booleanType
    | DATE                                         # dateType
    ;

dropTableStmt
    : DROP TABLE identifier
    ;

createIndexStmt
    : CREATE INDEX identifier ON identifier '(' identifier ')'
    ;

dropIndexStmt
    : DROP INDEX identifier
    ;

// ---------------------------------------------------------------------------
// DML
// ---------------------------------------------------------------------------

insertStmt
    : INSERT INTO identifier ('(' identifier (',' identifier)* ')')?
      VALUES valuesRow (',' valuesRow)*
    ;

valuesRow
    : '(' expr (',' expr)* ')'
    ;

updateStmt
    : UPDATE identifier SET assignment (',' assignment)* whereClause?
    ;

assignment
    : identifier '=' expr
    ;

deleteStmt
    : DELETE FROM identifier whereClause?
    ;

// ---------------------------------------------------------------------------
// SELECT
// ---------------------------------------------------------------------------

selectStmt
    : SELECT selectItem (',' selectItem)*
      FROM tableRef joinClause*
      whereClause?
      groupByClause?
      havingClause?
      orderByClause?
    ;

selectItem
    : '*'                                          # allColumnsItem
    | identifier '.' '*'                           # tableColumnsItem
    | expr (AS? identifier)?                       # exprItem
    ;

tableRef
    : identifier (AS? identifier)?
    ;

// JOIN a secas equivale a INNER JOIN.
joinClause
    : INNER? JOIN tableRef ON expr
    ;

whereClause
    : WHERE expr
    ;

groupByClause
    : GROUP BY expr (',' expr)*
    ;

havingClause
    : HAVING expr
    ;

orderByClause
    : ORDER BY orderItem (',' orderItem)*
    ;

orderItem
    : expr (ASC | DESC)?
    ;

// ---------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------

// El orden de las alternativas define la precedencia (de mayor a menor):
//   primario > unario +/- > * / % > + - > comparacion, IS NULL, IN, EXISTS
//   > NOT > AND > OR
// IN (subconsulta) va antes que IN (lista): si no, "(SELECT ...)" se
// intentaria leer como una lista de expresiones.
//
// La comparacion usa nombres de token y no literales: NEQ tiene dos formas
// ('<>' | '!='), y si aqui se escribiera '<>' ANTLR crearia un token
// implicito aparte (T__0) que nunca llegaria como NEQ.
expr
    : primary                                                         # primaryExpr
    | op=('-' | '+') expr                                             # unaryExpr
    | expr op=('*' | '/' | '%') expr                                  # mulExpr
    | expr op=('+' | '-') expr                                        # addExpr
    | expr op=(EQ | NEQ | LT | LE | GT | GE) expr                     # compareExpr
    | expr IS NOT? NULL                                               # isNullExpr
    | expr NOT? IN '(' selectStmt ')'                                 # inSubqueryExpr
    | expr NOT? IN '(' expr (',' expr)* ')'                           # inListExpr
    | NOT? EXISTS '(' selectStmt ')'                                  # existsExpr
    | NOT expr                                                        # notExpr
    | expr AND expr                                                   # andExpr
    | expr OR expr                                                    # orExpr
    ;

primary
    : literal                                      # literalPrimary
    | aggregate                                    # aggregatePrimary
    | identifier '.' identifier                    # qualifiedColumnPrimary
    | identifier                                   # columnPrimary
    | '(' selectStmt ')'                           # scalarSubqueryPrimary
    | '(' expr ')'                                 # parenPrimary
    ;

aggregate
    : COUNT '(' '*' ')'                                        # countStarAggregate
    | fn=(COUNT | SUM | AVG | MIN | MAX) '(' expr ')'          # callAggregate
    ;

literal
    : INTEGER_LITERAL                              # intLiteral
    | FLOAT_LITERAL                                # floatLiteral
    | STRING_LITERAL                               # stringLiteral
    | DATE STRING_LITERAL                          # dateLiteral
    | (TRUE | FALSE)                               # boolLiteral
    | NULL                                         # nullLiteral
    ;

identifier
    : IDENTIFIER
    ;

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

// Palabras clave. Van antes de IDENTIFIER: ante un empate de longitud, ANTLR
// elige la primera regla definida.
SELECT   : 'SELECT';
FROM     : 'FROM';
WHERE    : 'WHERE';
INSERT   : 'INSERT';
INTO     : 'INTO';
VALUES   : 'VALUES';
UPDATE   : 'UPDATE';
SET      : 'SET';
DELETE   : 'DELETE';
CREATE   : 'CREATE';
DROP     : 'DROP';
TABLE    : 'TABLE';
INDEX    : 'INDEX';
ON       : 'ON';
PRIMARY  : 'PRIMARY';
KEY      : 'KEY';
AND      : 'AND';
OR       : 'OR';
NOT      : 'NOT';
IN       : 'IN';
EXISTS   : 'EXISTS';
IS       : 'IS';
NULL     : 'NULL';
TRUE     : 'TRUE';
FALSE    : 'FALSE';
ORDER    : 'ORDER';
GROUP    : 'GROUP';
BY       : 'BY';
ASC      : 'ASC';
DESC     : 'DESC';
HAVING   : 'HAVING';
INNER    : 'INNER';
JOIN     : 'JOIN';
AS       : 'AS';
COUNT    : 'COUNT';
SUM      : 'SUM';
AVG      : 'AVG';
MIN      : 'MIN';
MAX      : 'MAX';

// Tipos. INTEGER se acepta como sinonimo de INT.
INT      : 'INT' | 'INTEGER';
FLOAT    : 'FLOAT';
VARCHAR  : 'VARCHAR';
BOOLEAN  : 'BOOLEAN';
DATE     : 'DATE';

// Simbolos. Tienen nombre para que la vista de tokens muestre algo legible.
LPAREN   : '(';
RPAREN   : ')';
COMMA    : ',';
SEMI     : ';';
DOT      : '.';
STAR     : '*';
PLUS     : '+';
MINUS    : '-';
SLASH    : '/';
PERCENT  : '%';
EQ       : '=';
NEQ      : '<>' | '!=';
LT       : '<';
LE       : '<=';
GT       : '>';
GE       : '>=';

// Literales.
FLOAT_LITERAL   : [0-9]+ '.' [0-9]* | '.' [0-9]+;
INTEGER_LITERAL : [0-9]+;
// Comilla simple; una comilla dentro de la cadena se escribe ''.
STRING_LITERAL  : '\'' ( ~'\'' | '\'\'' )* '\'';

// Letras ASCII y latinas acentuadas (nombres como 'año' o 'situación').
IDENTIFIER : [a-z_À-ÿ] [a-z_0-9À-ÿ]*;

// Comentarios y espacios.
LINE_COMMENT  : '--' ~[\r\n]* -> channel(HIDDEN);
BLOCK_COMMENT : '/*' .*? '*/' -> channel(HIDDEN);
WS            : [ \t\r\n]+ -> skip;

// Errores lexicos. Van al canal oculto para que el parser no los vea; el
// driver los recorre despues y emite LEX001/LEX002 con su posicion exacta.
//
// UNTERMINATED_STRING es un prefijo de STRING_LITERAL, asi que solo gana
// cuando falta la comilla de cierre (si no, STRING_LITERAL es mas largo).
UNTERMINATED_STRING : '\'' ( ~'\'' | '\'\'' )* -> channel(HIDDEN);
ERROR_CHAR          : . -> channel(HIDDEN);
