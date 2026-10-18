#include "frame_layout.h"

#include <algorithm>
#include <vector>

#include "semantic/scope.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using semantic::ClassSymbol;
using semantic::FunctionSymbol;
using semantic::StorageKind;
using semantic::Symbol;
using semantic::SymbolKind;

namespace {

constexpr int kWordSize = 4;
constexpr int kFirstParamOffset = 8;   // fp + 8: parametro 0
constexpr int kFirstLocalOffset = -8;  // fp - 8: local 0

// `base` si no esta en `used`; si no, base + separador + 1, 2, ... hasta
// encontrar uno libre. Lo marca como usado.
std::string uniqueName(const std::string& base, const std::string& separator,
                       std::set<std::string>& used) {
    std::string name = base;
    for (int n = 1; used.count(name) > 0; n++) name = base + separator + std::to_string(n);
    used.insert(name);
    return name;
}

}  // namespace

void FrameLayout::run(Program& program, semantic::SymbolTable& /*table*/) {
    globals_.clear();
    used_labels_.clear();
    laid_out_classes_.clear();
    top_level_ = FrameState();
    walkStatements(program.statements, top_level_);
}

void FrameLayout::walkStatements(const std::vector<StatementPtr>& statements,
                                 FrameState& state) {
    for (const auto& stmt : statements) walkStatement(stmt.get(), state);
}

void FrameLayout::walkStatement(Statement* stmt, FrameState& state) {
    if (auto* n = dynamic_cast<VariableDeclaration*>(stmt)) return placeVariable(n->symbol, state);
    if (auto* n = dynamic_cast<ConstantDeclaration*>(stmt)) return placeVariable(n->symbol, state);
    if (auto* n = dynamic_cast<FunctionDeclaration*>(stmt)) {
        return layoutFunction(n, state.fn, nullptr);
    }
    if (auto* n = dynamic_cast<ClassDeclaration*>(stmt)) {
        auto* cls = dynamic_cast<ClassSymbol*>(n->symbol);
        layoutClassFields(cls);
        for (const auto& member : n->members) {
            if (auto* method = dynamic_cast<FunctionDeclaration*>(member.get())) {
                layoutFunction(method, nullptr, cls);
            }
        }
        return;
    }
    if (auto* n = dynamic_cast<Block*>(stmt)) return walkStatements(n->statements, state);
    if (auto* n = dynamic_cast<IfStatement*>(stmt)) {
        walkStatements(n->then_block->statements, state);
        if (n->else_block) walkStatements(n->else_block->statements, state);
        return;
    }
    if (auto* n = dynamic_cast<WhileStatement*>(stmt)) {
        return walkStatements(n->body->statements, state);
    }
    if (auto* n = dynamic_cast<DoWhileStatement*>(stmt)) {
        return walkStatements(n->body->statements, state);
    }
    if (auto* n = dynamic_cast<ForStatement*>(stmt)) {
        if (n->init) walkStatement(n->init.get(), state);
        return walkStatements(n->body->statements, state);
    }
    if (auto* n = dynamic_cast<ForeachStatement*>(stmt)) {
        // La variable del foreach no tiene nodo de declaracion propio: vive
        // en el scope del bucle (ver DeclarationCollector).
        placeVariable(n->scope->resolveLocal(n->var_name).get(), state);
        return walkStatements(n->body->statements, state);
    }
    if (auto* n = dynamic_cast<SwitchStatement*>(stmt)) {
        for (const auto& c : n->cases) walkStatements(c->statements, state);
        if (n->has_default) walkStatements(n->default_statements, state);
        return;
    }
    if (auto* n = dynamic_cast<TryCatchStatement*>(stmt)) {
        walkStatements(n->try_block->statements, state);
        placeVariable(n->catch_block->scope->resolveLocal(n->error_name).get(), state);
        return walkStatements(n->catch_block->statements, state);
    }
    // Asignaciones, print, return, break, continue, expresiones: no declaran nada.
}

void FrameLayout::placeVariable(Symbol* symbol, FrameState& state) {
    if (symbol == nullptr) return;
    symbol->size = kWordSize;

    if (state.fn == nullptr) {
        symbol->storage = StorageKind::Global;
        symbol->offset = 0;
        symbol->owner_function = nullptr;
        // '_' y no '@': las globales terminan siendo etiquetas de MIPS.
        symbol->ir_name = uniqueName("g_" + symbol->name, "_", state.used_names);
        globals_.push_back(symbol);
        return;
    }

    symbol->storage = StorageKind::Local;
    symbol->offset = kFirstLocalOffset - kWordSize * state.locals;
    symbol->owner_function = state.fn;
    state.locals++;
    symbol->ir_name = uniqueName(symbol->name, "@", state.used_names);
}

void FrameLayout::layoutFunction(FunctionDeclaration* decl, FunctionSymbol* enclosing,
                                 ClassSymbol* ownerClass) {
    auto* fn = dynamic_cast<FunctionSymbol*>(decl->symbol);
    if (fn == nullptr) return;

    fn->is_method = ownerClass != nullptr;
    fn->nesting_depth = enclosing != nullptr ? enclosing->nesting_depth + 1 : 0;

    std::string base;
    if (ownerClass != nullptr) {
        base = ownerClass->name + "_" + decl->name;
    } else if (enclosing != nullptr) {
        base = enclosing->label + "_" + decl->name;
    } else {
        base = "fn_" + decl->name;
    }
    // Dos funciones con el mismo nombre en bloques distintos (sombreado)
    // no pueden compartir etiqueta.
    fn->label = uniqueName(base, "_", used_labels_);

    FrameState state;
    state.fn = fn;

    int index = fn->is_method ? 1 : 0;  // en metodos, 'this' ocupa el parametro 0
    for (const auto& param : decl->params) {
        Symbol* p = fn->function_scope->resolveLocal(param.name).get();
        if (p == nullptr) continue;
        p->storage = StorageKind::Param;
        p->size = kWordSize;
        p->offset = kFirstParamOffset + kWordSize * index;
        p->owner_function = fn;
        p->ir_name = uniqueName(param.name, "@", state.used_names);
        index++;
    }

    walkStatements(decl->body->statements, state);
    fn->frame_size = kWordSize * state.locals;
}

void FrameLayout::layoutClassFields(ClassSymbol* cls) {
    if (cls == nullptr || laid_out_classes_.count(cls) > 0) return;
    // Base primero: los atributos heredados quedan en las mismas posiciones
    // que en la clase base (despacho estatico sin vtables).
    if (cls->base_class != nullptr) layoutClassFields(cls->base_class);
    laid_out_classes_.insert(cls);

    int offset = cls->base_class != nullptr ? cls->base_class->instance_size : 0;
    if (cls->class_scope == nullptr) {
        cls->instance_size = offset;
        return;
    }

    std::vector<Symbol*> fields;
    for (const auto& entry : cls->class_scope->symbols()) {
        Symbol* sym = entry.second.get();
        if (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Constant) {
            fields.push_back(sym);
        }
    }
    std::sort(fields.begin(), fields.end(), [](const Symbol* a, const Symbol* b) {
        if (a->declared_line != b->declared_line) return a->declared_line < b->declared_line;
        return a->declared_column < b->declared_column;
    });

    for (Symbol* field : fields) {
        field->storage = StorageKind::Field;
        field->offset = offset;
        field->size = kWordSize;
        field->owner_function = nullptr;
        offset += kWordSize;
    }
    cls->instance_size = offset;
}

}  // namespace codegen
}  // namespace compiscript
