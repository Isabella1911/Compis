#ifndef COMPISCRIPT_AST_NODES_H
#define COMPISCRIPT_AST_NODES_H

// Jerarquia del AST de Compiscript. Cada nodo corresponde a una regla real
// de grammar/Compiscript.g4 (verificado linea por linea contra el .g4, no
// contra suposiciones). Los campos resolved_type/symbol/scope quedan como
// punteros no-propietarios en null: la Etapa 2 los llena, no hace falta
// tocar esta jerarquia de nuevo para eso.

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace compiscript {

// Forward declarations de las clases reales de src/semantic/ (Symbol y
// Scope ya existen ahi; Type todavia no). Alcanza con la declaracion
// adelantada porque AstNode solo guarda punteros no propietarios: si
// nodes.h incluyera semantic/symbol.h de verdad, se generaria un include
// circular (semantic/symbol.h ya incluye ast/nodes.h para TypeAnnotation).
namespace semantic {
class Symbol;
class Scope;
}  // namespace semantic

namespace ast {

// Sistema de tipos: todavia no existe ni siquiera como forward declaration
// util en otro lado, se deja el placeholder local hasta que se construya.
class Type;

class AstNode {
public:
    int line = 0;
    int column = 0;

    // Se llenan en etapas posteriores. No propietarios.
    Type* resolved_type = nullptr;
    semantic::Symbol* symbol = nullptr;
    semantic::Scope* scope = nullptr;

    virtual ~AstNode() = default;
};

class Statement : public AstNode {};
class Expression : public AstNode {};

using AstNodePtr = std::shared_ptr<AstNode>;
using StatementPtr = std::shared_ptr<Statement>;
using ExpressionPtr = std::shared_ptr<Expression>;

// ---------------------------------------------------------------------
// Anotaciones de tipo (type: baseType ('[' ']')*)
// ---------------------------------------------------------------------

class TypeAnnotation : public AstNode {};
using TypeAnnotationPtr = std::shared_ptr<TypeAnnotation>;

// boolean | integer | string | Identifier(nombre de clase)
class NamedTypeAnnotation : public TypeAnnotation {
public:
    std::string name;
    explicit NamedTypeAnnotation(std::string n) : name(std::move(n)) {}
};

// cada '[]' de la regla `type` anida un nivel
class ArrayTypeAnnotation : public TypeAnnotation {
public:
    TypeAnnotationPtr element;
    explicit ArrayTypeAnnotation(TypeAnnotationPtr elem) : element(std::move(elem)) {}
};

// ---------------------------------------------------------------------
// Parametro de funcion: `parameter: Identifier (':' type)?;`
// No es Statement ni Expression, es un fragmento auxiliar de FunctionDeclaration.
// ---------------------------------------------------------------------

struct Parameter {
    std::string name;
    TypeAnnotationPtr declared_type;  // null si no se anoto en el codigo fuente
    int line = 0;
    int column = 0;
};

// ---------------------------------------------------------------------
// Program
// ---------------------------------------------------------------------

class Program : public AstNode {
public:
    std::vector<StatementPtr> statements;
};
using ProgramPtr = std::shared_ptr<Program>;

// ---------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------

// ('let' | 'var') Identifier typeAnnotation? initializer? ';'
class VariableDeclaration : public Statement {
public:
    std::string name;
    TypeAnnotationPtr declared_type;   // null si no hay anotacion
    ExpressionPtr initializer;         // null si no hay inicializador
    std::string keyword;               // "let" o "var", por si se quiere diferenciar
};

// 'const' Identifier typeAnnotation? '=' expression ';'
// La gramatica ya obliga el '=', por eso initializer nunca es null aqui.
class ConstantDeclaration : public Statement {
public:
    std::string name;
    TypeAnnotationPtr declared_type;
    ExpressionPtr initializer;
};

// assignment: Identifier '=' expression ';'   (regla a nivel de STATEMENT,
// distinta de AssignmentExpression a nivel de expresion)
class AssignmentStatement : public Statement {
public:
    std::string target_name;
    ExpressionPtr value;
};

// assignment: expression '.' Identifier '=' expression ';'
class PropertyAssignment : public Statement {
public:
    ExpressionPtr object;
    std::string member_name;
    ExpressionPtr value;
};

// expressionStatement: expression ';'
class ExpressionStatement : public Statement {
public:
    ExpressionPtr expression;
};

// printStatement: 'print' '(' expression ')' ';'
// 'print' es una sentencia propia de la gramatica, NO una llamada a funcion.
class PrintStatement : public Statement {
public:
    ExpressionPtr expression;
};

// block: '{' statement* '}'
class Block : public Statement {
public:
    std::vector<StatementPtr> statements;
};
using BlockPtr = std::shared_ptr<Block>;

// ifStatement: 'if' '(' expression ')' block ('else' block)?
class IfStatement : public Statement {
public:
    ExpressionPtr condition;
    BlockPtr then_block;
    BlockPtr else_block;  // null si no hay 'else'
};

// whileStatement: 'while' '(' expression ')' block
class WhileStatement : public Statement {
public:
    ExpressionPtr condition;
    BlockPtr body;
};

// doWhileStatement: 'do' block 'while' '(' expression ')' ';'
class DoWhileStatement : public Statement {
public:
    BlockPtr body;
    ExpressionPtr condition;
};

// forStatement: 'for' '(' (variableDeclaration | assignment | ';') expression? ';' expression? ')' block
class ForStatement : public Statement {
public:
    StatementPtr init;          // VariableDeclaration | AssignmentStatement | null (si fue solo ';')
    ExpressionPtr condition;    // null si se omitio
    ExpressionPtr update;       // null si se omitio
    BlockPtr body;
};

// foreachStatement: 'foreach' '(' Identifier 'in' expression ')' block
class ForeachStatement : public Statement {
public:
    std::string var_name;
    ExpressionPtr iterable;
    BlockPtr body;
};

// switchCase: 'case' expression ':' statement*
// No es Statement de programa: es un fragmento auxiliar de SwitchStatement.
class SwitchCase : public AstNode {
public:
    ExpressionPtr expression;
    std::vector<StatementPtr> statements;
};
using SwitchCasePtr = std::shared_ptr<SwitchCase>;

// switchStatement: 'switch' '(' expression ')' '{' switchCase* defaultCase? '}'
class SwitchStatement : public Statement {
public:
    ExpressionPtr subject;
    std::vector<SwitchCasePtr> cases;
    bool has_default = false;
    std::vector<StatementPtr> default_statements;  // vacio si has_default es false
};

// tryCatchStatement: 'try' block 'catch' '(' Identifier ')' block
// El parametro del catch no lleva anotacion en la gramatica: se fija tipo
// `string` (ver README / decisiones de lenguaje).
class TryCatchStatement : public Statement {
public:
    BlockPtr try_block;
    std::string error_name;
    BlockPtr catch_block;
};

// returnStatement: 'return' expression? ';'
class ReturnStatement : public Statement {
public:
    ExpressionPtr value;  // null si es 'return;' sin valor
};

// breakStatement: 'break' ';'
class BreakStatement : public Statement {};

// continueStatement: 'continue' ';'
class ContinueStatement : public Statement {};

// functionDeclaration: 'function' Identifier '(' parameters? ')' (':' type)? block
// El constructor de una clase no tiene palabra reservada propia: es un
// FunctionDeclaration cuyo name es literalmente "constructor".
class FunctionDeclaration : public Statement {
public:
    std::string name;
    std::vector<Parameter> params;
    TypeAnnotationPtr return_type;  // null si no se anoto (se asume void en Etapa 2)
    BlockPtr body;
};
using FunctionDeclarationPtr = std::shared_ptr<FunctionDeclaration>;

// classDeclaration: 'class' Identifier (':' Identifier)? '{' classMember* '}'
// classMember: functionDeclaration | variableDeclaration | constantDeclaration
// La herencia usa ':' , no 'extends'.
class ClassDeclaration : public Statement {
public:
    std::string name;
    std::optional<std::string> base_name;  // nullopt si no hay herencia
    std::vector<StatementPtr> members;     // FunctionDeclaration | VariableDeclaration | ConstantDeclaration
};

// ---------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------

// AssignExpr: lhs=leftHandSide '=' assignmentExpr
// La asignacion es expresion: 'x = y = 3' es legal y se pliega a la derecha
// (el operando derecho es a su vez un assignmentExpr).
class AssignmentExpression : public Expression {
public:
    ExpressionPtr target;
    ExpressionPtr value;
};

// PropertyAssignExpr: lhs=leftHandSide '.' Identifier '=' assignmentExpr
class PropertyAssignExpr : public Expression {
public:
    ExpressionPtr object;
    std::string member_name;
    ExpressionPtr value;
};

// TernaryExpr: logicalOrExpr ('?' expression ':' expression)?
class TernaryExpression : public Expression {
public:
    ExpressionPtr condition;
    ExpressionPtr then_expr;
    ExpressionPtr else_expr;
};

// || && == != < <= > >= + - * / %  (todas las cadenas de precedencia
// binarias se colapsan a este unico nodo)
class BinaryExpression : public Expression {
public:
    std::string op;
    ExpressionPtr left;
    ExpressionPtr right;
};

// - !
class UnaryExpression : public Expression {
public:
    std::string op;
    ExpressionPtr operand;
};

enum class LiteralKind { Integer, String, Boolean, Null };

class LiteralExpression : public Expression {
public:
    std::string value;   // texto crudo del literal (se convierte en Etapa 2/3)
    LiteralKind kind;
};

// arrayLiteral: '[' (expression (',' expression)*)? ']'
class ArrayLiteral : public Expression {
public:
    std::vector<ExpressionPtr> elements;
};

class IdentifierExpression : public Expression {
public:
    std::string name;
};

class ThisExpression : public Expression {};

// NewExpr: 'new' Identifier '(' arguments? ')'
class NewExpression : public Expression {
public:
    std::string class_name;
    std::vector<ExpressionPtr> arguments;
};

// CallExpr (suffixOp): '(' arguments? ')'
class CallExpression : public Expression {
public:
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;
};

// IndexExpr (suffixOp): '[' expression ']'
class ArrayAccessExpression : public Expression {
public:
    ExpressionPtr array;
    ExpressionPtr index;
};

// PropertyAccessExpr (suffixOp): '.' Identifier
class MemberAccessExpression : public Expression {
public:
    ExpressionPtr object;
    std::string member_name;
};

}  // namespace ast
}  // namespace compiscript

#endif  // COMPISCRIPT_AST_NODES_H
