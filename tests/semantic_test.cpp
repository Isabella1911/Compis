// Regresiones sobre AST: inspeccionan metadatos que el CLI no serializa por completo.
#include <algorithm>
#include <iostream>
#include "compiler/result.h"
#include "semantic/declaration_collector.h"
#include "semantic/inheritance_resolver.h"
#include "semantic/name_resolver.h"
#include "semantic/type_checker.h"
#include "semantic/control_flow_checker.h"
#include "semantic/closure_analyzer.h"

using namespace compiscript;
using namespace ast;
using namespace semantic;
static int checks = 0, failures = 0;
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; std::cerr << "FALLO " << __LINE__ << ": " << #c << '\n'; } } while (0)

template<class T> std::shared_ptr<T> node() {
    auto n = std::make_shared<T>(); n->line = n->column = 1; return n;
}
TypeAnnotationPtr named(const std::string& name) { return std::make_shared<NamedTypeAnnotation>(name); }
ExpressionPtr integer() { auto n=node<LiteralExpression>(); n->kind=LiteralKind::Integer; n->value="1"; return n; }
auto id(const std::string& name) { auto n=node<IdentifierExpression>(); n->name=name; return n; }
auto print(ExpressionPtr expression) { auto n=node<PrintStatement>(); n->expression=expression; return n; }
auto variable(const std::string& name, ExpressionPtr init=integer()) {
    auto n=node<VariableDeclaration>(); n->name=name; n->initializer=init; return n;
}
auto function(const std::string& name, std::vector<StatementPtr> body={}) {
    auto n=node<FunctionDeclaration>(); n->name=name; n->body=node<Block>(); n->body->statements=std::move(body); return n;
}
auto klass(const std::string& name, std::vector<StatementPtr> members={}) {
    auto n=node<ClassDeclaration>(); n->name=name; n->members=std::move(members); return n;
}
compiler::CompilationResult analyze(std::vector<StatementPtr> statements) {
    compiler::CompilationResult result;
    result.ast=node<Program>(); result.ast->statements=std::move(statements);
    diagnostics::DiagnosticReporter reporter;
    DeclarationCollector(result.symbol_table,reporter).run(*result.ast);
    InheritanceResolver(reporter).run(*result.ast);
    NameResolver(reporter).run(*result.ast);
    TypeChecker(reporter).run(*result.ast);
    ControlFlowChecker(reporter).run(*result.ast);
    ClosureAnalyzer().run(*result.ast);
    result.diagnostics=reporter.sorted(); return result;
}
FunctionSymbol* symbol(const FunctionDeclarationPtr& f) { return dynamic_cast<FunctionSymbol*>(f->symbol); }
bool has(const compiler::CompilationResult& r,const std::string& code) {
    return std::any_of(r.diagnostics.begin(),r.diagnostics.end(),[&](const auto& d){return d.code==code;});
}
int main() {
    {
        auto first=variable("x"), duplicate=variable("x");
        auto use=id("x"); auto r=analyze({first,duplicate,print(use)});
        CHECK(has(r,"SEM002")); CHECK(first->symbol!=duplicate->symbol);
        CHECK(use->symbol==first->symbol); CHECK(duplicate->symbol->is_rejected);
        CHECK(duplicate->symbol->resolved_type->kind==TypeKind::Integer);
        CHECK(r.symbol_table.global()->rejectedSymbols().size()==1);
    }
    {
        auto first=function("f"), duplicate=function("f");
        first->params={{"x",named("integer"),1,1}};
        duplicate->params={{"x",named("string"),1,1}};
        auto a=klass("A"), b=klass("A");
        auto r=analyze({first,duplicate,a,b});
        CHECK(has(r,"SEM002"));
        CHECK(duplicate->body->scope->owner==duplicate->symbol);
        CHECK(symbol(first)->resolved_type->param_types[0]->kind==TypeKind::Integer);
        CHECK(symbol(duplicate)->resolved_type->param_types[0]->kind==TypeKind::String);
        CHECK(dynamic_cast<ClassSymbol*>(b->symbol)->class_scope->owner==b->symbol);
        CHECK(a->symbol!=b->symbol);
    }
    {
        auto f=function("f"); f->params={{"x",named("integer"),1,1},{"x",named("string"),1,2}};
        auto r=analyze({f});
        CHECK(has(r,"SEM002"));
        CHECK(f->body->scope->resolveLocal("x")->resolved_type->kind==TypeKind::Integer);
        CHECK(f->body->scope->rejectedSymbols()[0]->resolved_type->kind==TypeKind::String);
    }
    {
        auto f=function("f"); f->params={{"x",nullptr,1,1}};
        auto r=analyze({f}); CHECK(has(r,"SEM011"));
        CHECK(symbol(f)->resolved_type->param_types[0]->kind==TypeKind::Error);
        CHECK(symbol(f)->resolved_type->equals(*symbol(f)->resolved_type));
        Type incomplete; incomplete.kind=TypeKind::Function; incomplete.param_types={nullptr};
        incomplete.return_type=makeVoidType(); CHECK(!incomplete.equals(incomplete));
        CHECK(makeFunctionType({nullptr},nullptr)->param_types[0]!=nullptr);
    }
    {
        auto global=variable("global"); auto local=variable("x");
        auto c=function("c",{print(id("x")),print(id("p")),print(id("global")),print(id("a")),print(id("A"))});
        auto b=function("b",{c}); auto a=function("a",{local,b});
        a->params={{"p",named("integer"),1,1}};
        auto r=analyze({global,klass("A"),a}); CHECK(r.success());
        CHECK(symbol(a)->captured.empty()); CHECK(symbol(b)->captured.size()==2);
        CHECK(symbol(c)->captured==symbol(b)->captured);
        CHECK(symbol(c)->captured[0]==local->symbol);
        CHECK(symbol(c)->captured[1]==a->body->scope->resolveLocal("p").get());
        ClosureAnalyzer().run(*r.ast); CHECK(symbol(b)->captured.size()==2);
    }
    {
        auto local=variable("x"); auto innerLocal=variable("x");
        auto inner=function("inner",{innerLocal,print(id("x"))});
        auto outer=function("outer",{local,inner}); auto r=analyze({outer});
        CHECK(r.success()); CHECK(symbol(inner)->captured.empty());
    }
    {
        auto receiver=node<ThisExpression>();
        auto inner=function("inner",{print(receiver)}); auto middle=function("middle",{inner});
        auto method=function("method",{middle}); auto a=klass("A",{method});
        auto r=analyze({a}); CHECK(r.success());
        CHECK(symbol(method)->captured_this==nullptr);
        CHECK(symbol(middle)->captured_this==a->symbol);
        CHECK(symbol(inner)->captured_this==a->symbol);
        CHECK(symbol(inner)->captured.empty());
    }
    {
        auto x=variable("x"); auto method=function("method",{print(id("x"))});
        auto a=klass("A",{method}); auto outer=function("outer",{x,a});
        auto r=analyze({outer}); CHECK(r.success());
        CHECK(symbol(method)->captured.size()==1); CHECK(symbol(method)->captured[0]==x->symbol);
        CHECK(symbol(outer)->captured.empty());
    }
    {
        auto t=node<TryCatchStatement>(); t->error_name="e";
        t->try_block=node<Block>(); t->catch_block=node<Block>();
        auto r=analyze({t}); CHECK(r.success());
        CHECK(t->catch_block->scope->resolveLocal("e")->resolved_type->kind==TypeKind::String);
    }
    std::cout << checks << " checks semanticos, " << failures << " fallos\n";
    return failures ? 1 : 0;
}
