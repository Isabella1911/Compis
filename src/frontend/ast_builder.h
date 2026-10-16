#ifndef COMPISCRIPT_FRONTEND_AST_BUILDER_H
#define COMPISCRIPT_FRONTEND_AST_BUILDER_H

// Visitor que transforma el Parse Tree de ANTLR (CompiscriptParser::*Context)
// en el AST propio definido en ast/nodes.h. Cada visitXxx retorna un nodo:
// nunca imprime, nunca valida (eso es trabajo de la Etapa 3).
//
// Extiende la interfaz PURA compiscript::CompiscriptVisitor (no
// CompiscriptBaseVisitor) a proposito: asi el compilador obliga a
// implementar los 50 metodos y no hay manera de que una regla nueva del
// .g4 quede silenciosamente sin traducir.

#include <any>

#include "CompiscriptVisitor.h"
#include "ast/nodes.h"

namespace compiscript {
namespace frontend {

class AstBuilder : public compiscript::CompiscriptVisitor {
public:
    // Punto de entrada: parsea un ProgramContext completo.
    ast::ProgramPtr build(CompiscriptParser::ProgramContext* ctx);

    std::any visitProgram(CompiscriptParser::ProgramContext* ctx) override;
    std::any visitStatement(CompiscriptParser::StatementContext* ctx) override;
    std::any visitBlock(CompiscriptParser::BlockContext* ctx) override;
    std::any visitVariableDeclaration(CompiscriptParser::VariableDeclarationContext* ctx) override;
    std::any visitConstantDeclaration(CompiscriptParser::ConstantDeclarationContext* ctx) override;
    std::any visitTypeAnnotation(CompiscriptParser::TypeAnnotationContext* ctx) override;
    std::any visitInitializer(CompiscriptParser::InitializerContext* ctx) override;
    std::any visitAssignment(CompiscriptParser::AssignmentContext* ctx) override;
    std::any visitExpressionStatement(CompiscriptParser::ExpressionStatementContext* ctx) override;
    std::any visitPrintStatement(CompiscriptParser::PrintStatementContext* ctx) override;
    std::any visitIfStatement(CompiscriptParser::IfStatementContext* ctx) override;
    std::any visitWhileStatement(CompiscriptParser::WhileStatementContext* ctx) override;
    std::any visitDoWhileStatement(CompiscriptParser::DoWhileStatementContext* ctx) override;
    std::any visitForStatement(CompiscriptParser::ForStatementContext* ctx) override;
    std::any visitForeachStatement(CompiscriptParser::ForeachStatementContext* ctx) override;
    std::any visitBreakStatement(CompiscriptParser::BreakStatementContext* ctx) override;
    std::any visitContinueStatement(CompiscriptParser::ContinueStatementContext* ctx) override;
    std::any visitReturnStatement(CompiscriptParser::ReturnStatementContext* ctx) override;
    std::any visitTryCatchStatement(CompiscriptParser::TryCatchStatementContext* ctx) override;
    std::any visitSwitchStatement(CompiscriptParser::SwitchStatementContext* ctx) override;
    std::any visitSwitchCase(CompiscriptParser::SwitchCaseContext* ctx) override;
    std::any visitDefaultCase(CompiscriptParser::DefaultCaseContext* ctx) override;
    std::any visitFunctionDeclaration(CompiscriptParser::FunctionDeclarationContext* ctx) override;
    std::any visitParameters(CompiscriptParser::ParametersContext* ctx) override;
    std::any visitParameter(CompiscriptParser::ParameterContext* ctx) override;
    std::any visitClassDeclaration(CompiscriptParser::ClassDeclarationContext* ctx) override;
    std::any visitClassMember(CompiscriptParser::ClassMemberContext* ctx) override;
    std::any visitExpression(CompiscriptParser::ExpressionContext* ctx) override;
    std::any visitAssignExpr(CompiscriptParser::AssignExprContext* ctx) override;
    std::any visitPropertyAssignExpr(CompiscriptParser::PropertyAssignExprContext* ctx) override;
    std::any visitExprNoAssign(CompiscriptParser::ExprNoAssignContext* ctx) override;
    std::any visitTernaryExpr(CompiscriptParser::TernaryExprContext* ctx) override;
    std::any visitLogicalOrExpr(CompiscriptParser::LogicalOrExprContext* ctx) override;
    std::any visitLogicalAndExpr(CompiscriptParser::LogicalAndExprContext* ctx) override;
    std::any visitEqualityExpr(CompiscriptParser::EqualityExprContext* ctx) override;
    std::any visitRelationalExpr(CompiscriptParser::RelationalExprContext* ctx) override;
    std::any visitAdditiveExpr(CompiscriptParser::AdditiveExprContext* ctx) override;
    std::any visitMultiplicativeExpr(CompiscriptParser::MultiplicativeExprContext* ctx) override;
    std::any visitUnaryExpr(CompiscriptParser::UnaryExprContext* ctx) override;
    std::any visitPrimaryExpr(CompiscriptParser::PrimaryExprContext* ctx) override;
    std::any visitLiteralExpr(CompiscriptParser::LiteralExprContext* ctx) override;
    std::any visitLeftHandSide(CompiscriptParser::LeftHandSideContext* ctx) override;
    std::any visitIdentifierExpr(CompiscriptParser::IdentifierExprContext* ctx) override;
    std::any visitNewExpr(CompiscriptParser::NewExprContext* ctx) override;
    std::any visitThisExpr(CompiscriptParser::ThisExprContext* ctx) override;
    // visitCallExpr/visitIndexExpr/visitPropertyAccessExpr: los sufijos de
    // leftHandSide no cargan por si solos el operando al que se aplican
    // (el ".foo" de suffixOp no sabe sobre que objeto actua), asi que
    // visitLeftHandSide arma la cadena manualmente sin pasar por aqui.
    // Estos overrides existen solo para satisfacer la interfaz y nunca se
    // invocan en el pipeline normal.
    std::any visitCallExpr(CompiscriptParser::CallExprContext* ctx) override;
    std::any visitIndexExpr(CompiscriptParser::IndexExprContext* ctx) override;
    std::any visitPropertyAccessExpr(CompiscriptParser::PropertyAccessExprContext* ctx) override;
    // visitArguments/visitParameters: listas puras, el padre (CallExpr,
    // NewExpr, FunctionDeclaration) extrae los hijos directamente en vez
    // de invocar estos metodos.
    std::any visitArguments(CompiscriptParser::ArgumentsContext* ctx) override;
    std::any visitArrayLiteral(CompiscriptParser::ArrayLiteralContext* ctx) override;
    std::any visitType(CompiscriptParser::TypeContext* ctx) override;
    std::any visitBaseType(CompiscriptParser::BaseTypeContext* ctx) override;

private:
    template <typename OperandCtx>
    ast::ExpressionPtr foldLeftBinary(antlr4::ParserRuleContext* ctx,
                                       const std::vector<OperandCtx*>& operands);

    ast::ExpressionPtr asExpr(std::any result);
    ast::StatementPtr asStmt(std::any result);
    ast::TypeAnnotationPtr asType(std::any result);
    ast::BlockPtr asBlock(std::any result);
};

}  // namespace frontend
}  // namespace compiscript

#endif  // COMPISCRIPT_FRONTEND_AST_BUILDER_H
