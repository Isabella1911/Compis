// Parte 2: clases, new, atributos, metodos y this.
// Ver el documento de reparto de la parte 2, seccion 2.3.

#include <vector>

#include "tac_generator.h"

namespace compiscript {
namespace codegen {

using namespace compiscript::ast;
using ir::Operand;
using semantic::ClassSymbol;
using semantic::FunctionSymbol;
using semantic::StorageKind;

namespace {

constexpr int kThisOffset = 8;  // fp + 8: 'this' es el parametro 0
constexpr int kVtableOffset = 0;  // palabra 0 del objeto: puntero a la vtable

// Inicializadores de atributos propios de una clase, en orden de offset.
void collectOwnFieldInits(ClassDeclaration* decl,
                          std::vector<std::pair<semantic::Symbol*, Expression*>>& out) {
    if (decl == nullptr) return;
    for (const auto& member : decl->members) {
        if (auto* var = dynamic_cast<VariableDeclaration*>(member.get())) {
            if (var->symbol != nullptr && var->symbol->storage == StorageKind::Field) {
                out.push_back({var->symbol, var->initializer.get()});
            }
        } else if (auto* cnst = dynamic_cast<ConstantDeclaration*>(member.get())) {
            if (cnst->symbol != nullptr && cnst->symbol->storage == StorageKind::Field) {
                out.push_back({cnst->symbol, cnst->initializer.get()});
            }
        }
    }
}

}  // namespace

void TacGenerator::genClassDeclaration(ClassDeclaration* n) {
    auto* cls = dynamic_cast<ClassSymbol*>(n->symbol);
    if (cls != nullptr) class_decls_[cls] = n;

    // Los offsets de atributos ya los fijo FrameLayout. Aqui solo se
    // generan los metodos/constructor: la etiqueta ya es Clase_metodo y
    // 'this' ya esta contemplado en los offsets de parametros.
    for (const auto& member : n->members) {
        if (auto* method = dynamic_cast<FunctionDeclaration*>(member.get())) {
            genFunctionDeclaration(method);
        }
    }
}

Operand TacGenerator::genNew(NewExpression* n) {
    auto* cls = dynamic_cast<ClassSymbol*>(n->symbol);
    if (cls == nullptr) return todo("parte 2", "new " + n->class_name);

    Operand obj = emitAlloc(Operand::makeInt(cls->instance_size));
    // La vtable es la de la clase real del objeto: es lo que permite que
    // `let a: Animal = new Perro()` despache a.hablar() a Perro_hablar.
    emitStore(obj, Operand::makeInt(kVtableOffset), Operand::makeLabel(cls->vtable_label));

    // Inicializadores de atributos: base primero, despues la derivada.
    // Sin inicializador se escribe 0 (null/false/0).
    std::vector<ClassSymbol*> chain;
    for (ClassSymbol* c = cls; c != nullptr; c = c->base_class) chain.push_back(c);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        auto found = class_decls_.find(*it);
        if (found == class_decls_.end()) continue;
        std::vector<std::pair<semantic::Symbol*, Expression*>> inits;
        collectOwnFieldInits(found->second, inits);
        for (auto& [field, initExpr] : inits) {
            Operand value = initExpr ? genExpr(initExpr) : Operand::makeInt(0);
            emitStore(obj, Operand::makeInt(field->offset), value);
            release(value);
        }
    }

    // Constructor: se busca en la clase y en sus bases (igual que el
    // semántico con lookupMember). Despacho estatico: se llama a la
    // etiqueta del FunctionSymbol encontrado (p. ej. Animal_constructor
    // aunque el new sea `new Perro(...)`).
    FunctionSymbol* ctor = nullptr;
    for (ClassSymbol* c = cls; c != nullptr && ctor == nullptr; c = c->base_class) {
        if (c->class_scope == nullptr) continue;
        if (auto sym = c->class_scope->resolveLocal("constructor")) {
            ctor = dynamic_cast<FunctionSymbol*>(sym.get());
        }
    }

    if (ctor != nullptr) {
        std::vector<Operand> args;
        for (auto& a : n->arguments) args.push_back(genExpr(a.get()));
        for (auto it = args.rbegin(); it != args.rend(); ++it) emitParam(*it);
        for (auto& a : args) release(a);
        emitParam(obj);
        emitCall(ctor->label, static_cast<int>(n->arguments.size()) + 1, false);
    }

    return obj;
}

Operand TacGenerator::genMemberAccess(MemberAccessExpression* n) {
    if (n->symbol == nullptr) {
        return todo("parte 2", "acceso a '." + n->member_name + "'");
    }
    if (dynamic_cast<FunctionSymbol*>(n->symbol) != nullptr) {
        // Los metodos no escapan como valores (supuesto de la seccion 11).
        return todo("parte 2", "metodo '" + n->member_name + "' usado como valor");
    }

    Operand object = genExpr(n->object.get());
    release(object);
    return emitLoad(object, Operand::makeInt(n->symbol->offset));
}

Operand TacGenerator::genPropertyAssignment(Expression* object, semantic::Symbol* member,
                                            Expression* value) {
    if (member == nullptr) return todo("parte 2", "asignacion a atributo");

    Operand obj = genExpr(object);
    Operand val = genExpr(value);
    emitStore(obj, Operand::makeInt(member->offset), val);
    release(obj);
    // El valor de `obj.x = v` es v (permite encadenar). El llamador libera.
    return val;
}

Operand TacGenerator::genThis(ThisExpression* /*n*/) {
    // 'this' es el parametro 0 del metodo actual.
    return emitLoad(Operand::makeFramePtr(), Operand::makeInt(kThisOffset));
}

}  // namespace codegen
}  // namespace compiscript
