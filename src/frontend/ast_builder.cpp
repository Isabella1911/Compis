#include "ast_builder.h"

#include <stdexcept>

namespace compiscript {
namespace frontend {

using namespace compiscript::ast;

namespace {

void setPos(AstNode& node, antlr4::ParserRuleContext* ctx) {
    node.line = static_cast<int>(ctx->getStart()->getLine());
    node.column = static_cast<int>(ctx->getStart()->getCharPositionInLine()) + 1;
}

}  // namespace

ExpressionPtr AstBuilder::asExpr(std::any result) {
    return std::any_cast<ExpressionPtr>(result);
}

StatementPtr AstBuilder::asStmt(std::any result) {
    return std::any_cast<StatementPtr>(result);
}

TypeAnnotationPtr AstBuilder::asType(std::any result) {
    return std::any_cast<TypeAnnotationPtr>(result);
}

BlockPtr AstBuilder::asBlock(std::any result) {
    // visitBlock guarda el resultado como StatementPtr (misma convencion que
    // el resto de las sentencias), asi que hay que any_cast a ese tipo
    // primero y solo despues bajar a Block con dynamic_pointer_cast: un
    // any_cast<BlockPtr> directo falla porque std::any no hace conversion
    // polimorfica de shared_ptr, solo compara el tipo exacto almacenado.
    return std::dynamic_pointer_cast<Block>(std::any_cast<StatementPtr>(result));
}

template <typename OperandCtx>
ExpressionPtr AstBuilder::foldLeftBinary(antlr4::ParserRuleContext* ctx,
                                          const std::vector<OperandCtx*>& operands) {
    // La gramatica usa repeticion ( op operando )*, no recursion: hay que
    // plegar a la izquierda a mano o "a - b - c" queda mal asociado.
    ExpressionPtr node = asExpr(visit(operands[0]));
    for (size_t i = 1; i < operands.size(); i++) {
        std::string op = ctx->children[2 * i - 1]->getText();
        ExpressionPtr rhs = asExpr(visit(operands[i]));
        auto bin = std::make_shared<BinaryExpression>();
        bin->op = op;
        bin->left = node;
        bin->right = rhs;
        setPos(*bin, ctx);
        node = bin;
    }
    return node;
}

ast::ProgramPtr AstBuilder::build(CompiscriptParser::ProgramContext* ctx) {
    return std::any_cast<ProgramPtr>(visitProgram(ctx));
}

// ---------------------------------------------------------------------
// Program / statement / block
// ---------------------------------------------------------------------

std::any AstBuilder::visitProgram(CompiscriptParser::ProgramContext* ctx) {
    auto node = std::make_shared<Program>();
    setPos(*node, ctx);
    for (auto* stmtCtx : ctx->statement()) {
        node->statements.push_back(asStmt(visit(stmtCtx)));
    }
    return std::any(ProgramPtr(node));
}

std::any AstBuilder::visitStatement(CompiscriptParser::StatementContext* ctx) {
    // Alternancia pura, un solo hijo con contenido real: se delega.
    if (ctx->variableDeclaration()) return visit(ctx->variableDeclaration());
    if (ctx->constantDeclaration()) return visit(ctx->constantDeclaration());
    if (ctx->assignment()) return visit(ctx->assignment());
    if (ctx->functionDeclaration()) return visit(ctx->functionDeclaration());
    if (ctx->classDeclaration()) return visit(ctx->classDeclaration());
    if (ctx->expressionStatement()) return visit(ctx->expressionStatement());
    if (ctx->printStatement()) return visit(ctx->printStatement());
    if (ctx->block()) return visit(ctx->block());
    if (ctx->ifStatement()) return visit(ctx->ifStatement());
    if (ctx->whileStatement()) return visit(ctx->whileStatement());
    if (ctx->doWhileStatement()) return visit(ctx->doWhileStatement());
    if (ctx->forStatement()) return visit(ctx->forStatement());
    if (ctx->foreachStatement()) return visit(ctx->foreachStatement());
    if (ctx->tryCatchStatement()) return visit(ctx->tryCatchStatement());
    if (ctx->switchStatement()) return visit(ctx->switchStatement());
    if (ctx->breakStatement()) return visit(ctx->breakStatement());
    if (ctx->continueStatement()) return visit(ctx->continueStatement());
    if (ctx->returnStatement()) return visit(ctx->returnStatement());
    throw std::logic_error("statement sin alternativa reconocida");
}

std::any AstBuilder::visitBlock(CompiscriptParser::BlockContext* ctx) {
    auto node = std::make_shared<Block>();
    setPos(*node, ctx);
    for (auto* stmtCtx : ctx->statement()) {
        node->statements.push_back(asStmt(visit(stmtCtx)));
    }
    return std::any(StatementPtr(node));
}

// ---------------------------------------------------------------------
// Declaraciones
// ---------------------------------------------------------------------

std::any AstBuilder::visitVariableDeclaration(CompiscriptParser::VariableDeclarationContext* ctx) {
    auto node = std::make_shared<VariableDeclaration>();
    setPos(*node, ctx);
    node->name = ctx->Identifier()->getText();
    node->keyword = ctx->getStart()->getText();  // 'let' o 'var'
    if (ctx->typeAnnotation()) {
        node->declared_type = asType(visit(ctx->typeAnnotation()));
    }
    if (ctx->initializer()) {
        node->initializer = asExpr(visit(ctx->initializer()));
    }
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitConstantDeclaration(CompiscriptParser::ConstantDeclarationContext* ctx) {
    auto node = std::make_shared<ConstantDeclaration>();
    setPos(*node, ctx);
    node->name = ctx->Identifier()->getText();
    if (ctx->typeAnnotation()) {
        node->declared_type = asType(visit(ctx->typeAnnotation()));
    }
    // La gramatica obliga '=' expression: siempre presente.
    node->initializer = asExpr(visit(ctx->expression()));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitTypeAnnotation(CompiscriptParser::TypeAnnotationContext* ctx) {
    // ':' type -- se colapsa al type.
    return visit(ctx->type());
}

std::any AstBuilder::visitInitializer(CompiscriptParser::InitializerContext* ctx) {
    // '=' expression -- se colapsa a la expresion.
    return visit(ctx->expression());
}

std::any AstBuilder::visitAssignment(CompiscriptParser::AssignmentContext* ctx) {
    // Dos alternativas sin labels, distinguidas por cuantos `expression` hay:
    //   Identifier '=' expression ';'                     -> 1 expression
    //   expression '.' Identifier '=' expression ';'       -> 2 expressions
    if (ctx->expression().size() == 1) {
        auto node = std::make_shared<AssignmentStatement>();
        setPos(*node, ctx);
        node->target_name = ctx->Identifier()->getText();
        node->value = asExpr(visit(ctx->expression(0)));
        return std::any(StatementPtr(node));
    }
    auto node = std::make_shared<PropertyAssignment>();
    setPos(*node, ctx);
    node->object = asExpr(visit(ctx->expression(0)));
    node->member_name = ctx->Identifier()->getText();
    node->value = asExpr(visit(ctx->expression(1)));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitExpressionStatement(CompiscriptParser::ExpressionStatementContext* ctx) {
    auto node = std::make_shared<ExpressionStatement>();
    setPos(*node, ctx);
    node->expression = asExpr(visit(ctx->expression()));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitPrintStatement(CompiscriptParser::PrintStatementContext* ctx) {
    auto node = std::make_shared<PrintStatement>();
    setPos(*node, ctx);
    node->expression = asExpr(visit(ctx->expression()));
    return std::any(StatementPtr(node));
}

// ---------------------------------------------------------------------
// Control de flujo
// ---------------------------------------------------------------------

std::any AstBuilder::visitIfStatement(CompiscriptParser::IfStatementContext* ctx) {
    auto node = std::make_shared<IfStatement>();
    setPos(*node, ctx);
    node->condition = asExpr(visit(ctx->expression()));
    // block() es lista (0..2 segun haya 'else' o no): siempre hay al menos el then.
    node->then_block = asBlock(visit(ctx->block(0)));
    if (ctx->block().size() > 1) {
        node->else_block = asBlock(visit(ctx->block(1)));
    }
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitWhileStatement(CompiscriptParser::WhileStatementContext* ctx) {
    auto node = std::make_shared<WhileStatement>();
    setPos(*node, ctx);
    node->condition = asExpr(visit(ctx->expression()));
    node->body = asBlock(visit(ctx->block()));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitDoWhileStatement(CompiscriptParser::DoWhileStatementContext* ctx) {
    auto node = std::make_shared<DoWhileStatement>();
    setPos(*node, ctx);
    node->body = asBlock(visit(ctx->block()));
    node->condition = asExpr(visit(ctx->expression()));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitForStatement(CompiscriptParser::ForStatementContext* ctx) {
    // 'for' '(' (variableDeclaration | assignment | ';') expression? ';' expression? ')' block
    //
    // OJO: los dos `expression?` no son distinguibles por indice via el
    // accessor generado (expression(i) filtra por tipo entre los hijos
    // presentes, no por "slot" logico) cuando falta uno de los dos. Por
    // eso se recorren los hijos directos a mano para saber cual expression
    // es la condicion y cual el update.
    auto node = std::make_shared<ForStatement>();
    setPos(*node, ctx);

    if (ctx->variableDeclaration()) {
        node->init = asStmt(visit(ctx->variableDeclaration()));
    } else if (ctx->assignment()) {
        node->init = asStmt(visit(ctx->assignment()));
    }
    // si ninguno esta presente, el init fue el ';' literal -> node->init queda null

    // Recorrido de hijos directos para ubicar los dos expression? por posicion.
    int semicolonsSeen = 0;
    bool sawConditionExpr = false;
    for (auto* child : ctx->children) {
        auto* exprChild = dynamic_cast<CompiscriptParser::ExpressionContext*>(child);
        auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child);
        if (terminal != nullptr && terminal->getText() == ";") {
            semicolonsSeen++;
            continue;
        }
        if (exprChild == nullptr) continue;
        // El primer expression que aparece tras el primer ';' directo (o
        // tras el init si este consumio su propio ';' internamente) es la
        // condicion; el que aparece tras el segundo ';' directo es el update.
        if (semicolonsSeen <= 1 && !sawConditionExpr) {
            node->condition = asExpr(visit(exprChild));
            sawConditionExpr = true;
        } else {
            node->update = asExpr(visit(exprChild));
        }
    }

    node->body = asBlock(visit(ctx->block()));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitForeachStatement(CompiscriptParser::ForeachStatementContext* ctx) {
    auto node = std::make_shared<ForeachStatement>();
    setPos(*node, ctx);
    node->var_name = ctx->Identifier()->getText();
    node->iterable = asExpr(visit(ctx->expression()));
    node->body = asBlock(visit(ctx->block()));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitBreakStatement(CompiscriptParser::BreakStatementContext* ctx) {
    auto node = std::make_shared<BreakStatement>();
    setPos(*node, ctx);
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitContinueStatement(CompiscriptParser::ContinueStatementContext* ctx) {
    auto node = std::make_shared<ContinueStatement>();
    setPos(*node, ctx);
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitReturnStatement(CompiscriptParser::ReturnStatementContext* ctx) {
    auto node = std::make_shared<ReturnStatement>();
    setPos(*node, ctx);
    if (ctx->expression()) {
        node->value = asExpr(visit(ctx->expression()));
    }
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitTryCatchStatement(CompiscriptParser::TryCatchStatementContext* ctx) {
    auto node = std::make_shared<TryCatchStatement>();
    setPos(*node, ctx);
    node->try_block = asBlock(visit(ctx->block(0)));
    node->error_name = ctx->Identifier()->getText();
    node->catch_block = asBlock(visit(ctx->block(1)));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitSwitchStatement(CompiscriptParser::SwitchStatementContext* ctx) {
    auto node = std::make_shared<SwitchStatement>();
    setPos(*node, ctx);
    node->subject = asExpr(visit(ctx->expression()));
    for (auto* caseCtx : ctx->switchCase()) {
        node->cases.push_back(std::any_cast<SwitchCasePtr>(visit(caseCtx)));
    }
    if (ctx->defaultCase()) {
        node->has_default = true;
        for (auto* stmtCtx : ctx->defaultCase()->statement()) {
            node->default_statements.push_back(asStmt(visit(stmtCtx)));
        }
    }
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitSwitchCase(CompiscriptParser::SwitchCaseContext* ctx) {
    auto node = std::make_shared<SwitchCase>();
    setPos(*node, ctx);
    node->expression = asExpr(visit(ctx->expression()));
    for (auto* stmtCtx : ctx->statement()) {
        node->statements.push_back(asStmt(visit(stmtCtx)));
    }
    return std::any(SwitchCasePtr(node));
}

std::any AstBuilder::visitDefaultCase(CompiscriptParser::DefaultCaseContext* /*ctx*/) {
    // No se invoca directamente: visitSwitchStatement lee ctx->defaultCase()
    // y extrae sus statements el mismo, porque DefaultCase no necesita un
    // tipo de nodo propio (sus statements se vuelcan directo en
    // SwitchStatement::default_statements).
    return std::any();
}

// ---------------------------------------------------------------------
// Funciones y clases
// ---------------------------------------------------------------------

std::any AstBuilder::visitFunctionDeclaration(CompiscriptParser::FunctionDeclarationContext* ctx) {
    auto node = std::make_shared<FunctionDeclaration>();
    setPos(*node, ctx);
    node->name = ctx->Identifier()->getText();
    if (ctx->parameters()) {
        for (auto* paramCtx : ctx->parameters()->parameter()) {
            Parameter param;
            param.name = paramCtx->Identifier()->getText();
            param.line = static_cast<int>(paramCtx->getStart()->getLine());
            param.column = static_cast<int>(paramCtx->getStart()->getCharPositionInLine()) + 1;
            if (paramCtx->type()) {
                param.declared_type = asType(visit(paramCtx->type()));
            }
            node->params.push_back(std::move(param));
        }
    }
    if (ctx->type()) {
        node->return_type = asType(visit(ctx->type()));
    }
    node->body = asBlock(visit(ctx->block()));
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitParameters(CompiscriptParser::ParametersContext* /*ctx*/) {
    // No se invoca: visitFunctionDeclaration itera ctx->parameters()->parameter() a mano.
    return std::any();
}

std::any AstBuilder::visitParameter(CompiscriptParser::ParameterContext* /*ctx*/) {
    // No se invoca: visitFunctionDeclaration arma el struct Parameter directamente.
    return std::any();
}

std::any AstBuilder::visitClassDeclaration(CompiscriptParser::ClassDeclarationContext* ctx) {
    auto node = std::make_shared<ClassDeclaration>();
    setPos(*node, ctx);
    // Identifier() es lista: [0] = nombre de la clase, [1] = clase base si hay ':'.
    node->name = ctx->Identifier(0)->getText();
    if (ctx->Identifier().size() > 1) {
        node->base_name = ctx->Identifier(1)->getText();
    }
    for (auto* memberCtx : ctx->classMember()) {
        node->members.push_back(asStmt(visit(memberCtx)));
    }
    return std::any(StatementPtr(node));
}

std::any AstBuilder::visitClassMember(CompiscriptParser::ClassMemberContext* ctx) {
    if (ctx->functionDeclaration()) return visit(ctx->functionDeclaration());
    if (ctx->variableDeclaration()) return visit(ctx->variableDeclaration());
    if (ctx->constantDeclaration()) return visit(ctx->constantDeclaration());
    throw std::logic_error("classMember sin alternativa reconocida");
}

// ---------------------------------------------------------------------
// Expresiones -- cadenas de precedencia
// ---------------------------------------------------------------------

std::any AstBuilder::visitExpression(CompiscriptParser::ExpressionContext* ctx) {
    // expression: assignmentExpr -- se colapsa.
    return visit(ctx->assignmentExpr());
}

std::any AstBuilder::visitAssignExpr(CompiscriptParser::AssignExprContext* ctx) {
    auto node = std::make_shared<AssignmentExpression>();
    setPos(*node, ctx);
    node->target = asExpr(visit(ctx->lhs));
    node->value = asExpr(visit(ctx->assignmentExpr()));
    return std::any(ExpressionPtr(node));
}

std::any AstBuilder::visitPropertyAssignExpr(CompiscriptParser::PropertyAssignExprContext* ctx) {
    auto node = std::make_shared<PropertyAssignExpr>();
    setPos(*node, ctx);
    node->object = asExpr(visit(ctx->lhs));
    node->member_name = ctx->Identifier()->getText();
    node->value = asExpr(visit(ctx->assignmentExpr()));
    return std::any(ExpressionPtr(node));
}

std::any AstBuilder::visitExprNoAssign(CompiscriptParser::ExprNoAssignContext* ctx) {
    return visit(ctx->conditionalExpr());
}

std::any AstBuilder::visitTernaryExpr(CompiscriptParser::TernaryExprContext* ctx) {
    // logicalOrExpr ('?' expression ':' expression)?  -- se colapsa si no hay '?'.
    if (ctx->expression().empty()) {
        return visit(ctx->logicalOrExpr());
    }
    auto node = std::make_shared<TernaryExpression>();
    setPos(*node, ctx);
    node->condition = asExpr(visit(ctx->logicalOrExpr()));
    node->then_expr = asExpr(visit(ctx->expression(0)));
    node->else_expr = asExpr(visit(ctx->expression(1)));
    return std::any(ExpressionPtr(node));
}

std::any AstBuilder::visitLogicalOrExpr(CompiscriptParser::LogicalOrExprContext* ctx) {
    auto operands = ctx->logicalAndExpr();
    if (operands.size() == 1) return visit(operands[0]);
    return std::any(foldLeftBinary(ctx, operands));
}

std::any AstBuilder::visitLogicalAndExpr(CompiscriptParser::LogicalAndExprContext* ctx) {
    auto operands = ctx->equalityExpr();
    if (operands.size() == 1) return visit(operands[0]);
    return std::any(foldLeftBinary(ctx, operands));
}

std::any AstBuilder::visitEqualityExpr(CompiscriptParser::EqualityExprContext* ctx) {
    auto operands = ctx->relationalExpr();
    if (operands.size() == 1) return visit(operands[0]);
    return std::any(foldLeftBinary(ctx, operands));
}

std::any AstBuilder::visitRelationalExpr(CompiscriptParser::RelationalExprContext* ctx) {
    auto operands = ctx->additiveExpr();
    if (operands.size() == 1) return visit(operands[0]);
    return std::any(foldLeftBinary(ctx, operands));
}

std::any AstBuilder::visitAdditiveExpr(CompiscriptParser::AdditiveExprContext* ctx) {
    auto operands = ctx->multiplicativeExpr();
    if (operands.size() == 1) return visit(operands[0]);
    return std::any(foldLeftBinary(ctx, operands));
}

std::any AstBuilder::visitMultiplicativeExpr(CompiscriptParser::MultiplicativeExprContext* ctx) {
    auto operands = ctx->unaryExpr();
    if (operands.size() == 1) return visit(operands[0]);
    return std::any(foldLeftBinary(ctx, operands));
}

std::any AstBuilder::visitUnaryExpr(CompiscriptParser::UnaryExprContext* ctx) {
    // ('-' | '!') unaryExpr | primaryExpr
    if (ctx->primaryExpr()) {
        return visit(ctx->primaryExpr());
    }
    auto node = std::make_shared<UnaryExpression>();
    setPos(*node, ctx);
    node->op = ctx->getStart()->getText();
    node->operand = asExpr(visit(ctx->unaryExpr()));
    return std::any(ExpressionPtr(node));
}

std::any AstBuilder::visitPrimaryExpr(CompiscriptParser::PrimaryExprContext* ctx) {
    // literalExpr | leftHandSide | '(' expression ')'  -- las tres se colapsan.
    if (ctx->literalExpr()) return visit(ctx->literalExpr());
    if (ctx->leftHandSide()) return visit(ctx->leftHandSide());
    return visit(ctx->expression());
}

std::any AstBuilder::visitLiteralExpr(CompiscriptParser::LiteralExprContext* ctx) {
    auto node = std::make_shared<LiteralExpression>();
    setPos(*node, ctx);
    if (ctx->Literal() != nullptr) {
        // OJO: la gramatica define Literal, IntegerLiteral y StringLiteral
        // como reglas de lexer separadas, pero por el orden de declaracion
        // el lexer SIEMPRE emite el token "Literal" (nunca IntegerLiteral
        // ni StringLiteral por separado), asi que hay que distinguir
        // entero vs string inspeccionando el texto, no el tipo de token.
        std::string text = ctx->Literal()->getText();
        node->value = text;
        node->kind = (!text.empty() && text.front() == '"') ? LiteralKind::String
                                                              : LiteralKind::Integer;
    } else if (ctx->arrayLiteral()) {
        return visit(ctx->arrayLiteral());
    } else {
        // 'null' | 'true' | 'false'
        std::string text = ctx->getText();
        node->value = text;
        node->kind = (text == "null") ? LiteralKind::Null : LiteralKind::Boolean;
    }
    return std::any(ExpressionPtr(node));
}

std::any AstBuilder::visitArrayLiteral(CompiscriptParser::ArrayLiteralContext* ctx) {
    auto node = std::make_shared<ArrayLiteral>();
    setPos(*node, ctx);
    for (auto* exprCtx : ctx->expression()) {
        node->elements.push_back(asExpr(visit(exprCtx)));
    }
    return std::any(ExpressionPtr(node));
}

// ---------------------------------------------------------------------
// leftHandSide: primaryAtom (suffixOp)*
// Se pliega a la izquierda a mano: cada sufijo envuelve al resultado
// acumulado hasta ese punto (a.b[0].c() -> Call(Access(Index(Access(a,b),0),c))).
// ---------------------------------------------------------------------

std::any AstBuilder::visitLeftHandSide(CompiscriptParser::LeftHandSideContext* ctx) {
    ExpressionPtr node = asExpr(visit(ctx->primaryAtom()));
    for (auto* suffix : ctx->suffixOp()) {
        if (auto* call = dynamic_cast<CompiscriptParser::CallExprContext*>(suffix)) {
            auto callNode = std::make_shared<CallExpression>();
            setPos(*callNode, call);
            callNode->callee = node;
            if (call->arguments()) {
                for (auto* argCtx : call->arguments()->expression()) {
                    callNode->arguments.push_back(asExpr(visit(argCtx)));
                }
            }
            node = callNode;
        } else if (auto* index = dynamic_cast<CompiscriptParser::IndexExprContext*>(suffix)) {
            auto idxNode = std::make_shared<ArrayAccessExpression>();
            setPos(*idxNode, index);
            idxNode->array = node;
            idxNode->index = asExpr(visit(index->expression()));
            node = idxNode;
        } else if (auto* prop =
                       dynamic_cast<CompiscriptParser::PropertyAccessExprContext*>(suffix)) {
            auto propNode = std::make_shared<MemberAccessExpression>();
            setPos(*propNode, prop);
            propNode->object = node;
            propNode->member_name = prop->Identifier()->getText();
            node = propNode;
        }
    }
    return std::any(node);
}

std::any AstBuilder::visitIdentifierExpr(CompiscriptParser::IdentifierExprContext* ctx) {
    auto node = std::make_shared<IdentifierExpression>();
    setPos(*node, ctx);
    node->name = ctx->Identifier()->getText();
    return std::any(ExpressionPtr(node));
}

std::any AstBuilder::visitNewExpr(CompiscriptParser::NewExprContext* ctx) {
    auto node = std::make_shared<NewExpression>();
    setPos(*node, ctx);
    node->class_name = ctx->Identifier()->getText();
    if (ctx->arguments()) {
        for (auto* argCtx : ctx->arguments()->expression()) {
            node->arguments.push_back(asExpr(visit(argCtx)));
        }
    }
    return std::any(ExpressionPtr(node));
}

std::any AstBuilder::visitThisExpr(CompiscriptParser::ThisExprContext* ctx) {
    auto node = std::make_shared<ThisExpression>();
    setPos(*node, ctx);
    return std::any(ExpressionPtr(node));
}

std::any AstBuilder::visitCallExpr(CompiscriptParser::CallExprContext* /*ctx*/) {
    return std::any();  // ver comentario en el .h: visitLeftHandSide maneja esto.
}

std::any AstBuilder::visitIndexExpr(CompiscriptParser::IndexExprContext* /*ctx*/) {
    return std::any();
}

std::any AstBuilder::visitPropertyAccessExpr(CompiscriptParser::PropertyAccessExprContext* /*ctx*/) {
    return std::any();
}

std::any AstBuilder::visitArguments(CompiscriptParser::ArgumentsContext* /*ctx*/) {
    return std::any();  // ver comentario en el .h: el padre extrae los hijos directamente.
}

// ---------------------------------------------------------------------
// Tipos
// ---------------------------------------------------------------------

std::any AstBuilder::visitType(CompiscriptParser::TypeContext* ctx) {
    // type: baseType ('[' ']')*
    TypeAnnotationPtr node = asType(visit(ctx->baseType()));
    int arrayDepth = 0;
    for (auto* child : ctx->children) {
        auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child);
        if (terminal != nullptr && terminal->getText() == "[") {
            arrayDepth++;
        }
    }
    for (int i = 0; i < arrayDepth; i++) {
        auto wrapper = std::make_shared<ArrayTypeAnnotation>(node);
        setPos(*wrapper, ctx);
        node = wrapper;
    }
    return std::any(node);
}

std::any AstBuilder::visitBaseType(CompiscriptParser::BaseTypeContext* ctx) {
    // 'boolean' | 'integer' | 'string' | Identifier -- las 4 alternativas
    // son un solo token; el texto crudo ya es el nombre del tipo.
    auto node = std::make_shared<NamedTypeAnnotation>(ctx->getText());
    setPos(*node, ctx);
    return std::any(TypeAnnotationPtr(node));
}

}  // namespace frontend
}  // namespace compiscript
