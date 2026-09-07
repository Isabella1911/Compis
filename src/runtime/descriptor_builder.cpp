#include "runtime/descriptor_builder.h"

#include <algorithm>

#include "semantic/scope.h"
#include "semantic/symbol.h"

namespace compiscript {
namespace runtime {

const ClassDescriptor* DescriptorRegistry::findClass(const std::string& name) const {
    auto it = classes_.find(name);
    return it == classes_.end() ? nullptr : it->second.get();
}

const ArrayDescriptor* DescriptorRegistry::arrayOf(SlotKind element_kind) {
    const int key = static_cast<int>(element_kind);
    auto it = arrays_.find(key);
    if (it != arrays_.end()) {
        return it->second.get();
    }
    auto desc = std::make_unique<ArrayDescriptor>();
    desc->element_kind = element_kind;
    ArrayDescriptor* raw = desc.get();
    arrays_[key] = std::move(desc);
    return raw;
}

SlotKind DescriptorBuilder::slotKindFromType(const semantic::TypePtr& type) {
    if (!type) {
        return SlotKind::Primitive;
    }
    using semantic::TypeKind;
    switch (type->kind) {
        case TypeKind::Class:
        case TypeKind::Array:
            return SlotKind::Pointer;
        default:
            // Integer, Boolean, String, Null, Void, Function, Error, EmptyElement:
            // el mark no los sigue. Strings podrian pasar a Pointer si el
            // backend las coloca en el heap; hoy se tratan como plano.
            return SlotKind::Primitive;
    }
}

void DescriptorBuilder::collectClassFields(semantic::ClassSymbol* cls,
                                           ClassDescriptor& out,
                                           std::vector<semantic::ClassSymbol*>& visited) {
    if (cls == nullptr) {
        return;
    }
    for (semantic::ClassSymbol* seen : visited) {
        if (seen == cls) {
            return;  // ciclo de herencia: no reentrar
        }
    }
    visited.push_back(cls);

    if (cls->base_class != nullptr) {
        collectClassFields(cls->base_class, out, visited);
    }

    semantic::Scope* scope = cls->class_scope;
    if (scope == nullptr) {
        return;
    }
    // Orden estable por nombre: el Scope usa unordered_map.
    std::vector<FieldSlot> local;
    for (const auto& entry : scope->symbols()) {
        const semantic::SymbolPtr& sym = entry.second;
        if (!sym || sym->is_rejected) {
            continue;
        }
        if (sym->kind != semantic::SymbolKind::Variable &&
            sym->kind != semantic::SymbolKind::Constant) {
            continue;  // metodos no ocupan slot de instancia
        }
        FieldSlot slot;
        slot.name = sym->name;
        slot.kind = slotKindFromType(sym->resolved_type);
        local.push_back(std::move(slot));
    }
    std::sort(local.begin(), local.end(),
              [](const FieldSlot& a, const FieldSlot& b) { return a.name < b.name; });
    for (FieldSlot& slot : local) {
        slot.word_index = out.fields.size();
        out.fields.push_back(std::move(slot));
    }
}

void DescriptorBuilder::walkScope(semantic::Scope* scope, DescriptorRegistry& reg) {
    if (scope == nullptr) {
        return;
    }

    for (const auto& entry : scope->symbols()) {
        const semantic::SymbolPtr& sym = entry.second;
        if (!sym || sym->is_rejected) {
            continue;
        }

        if (auto* cls = dynamic_cast<semantic::ClassSymbol*>(sym.get())) {
            if (reg.classes_.count(cls->name) == 0) {
                auto desc = std::make_unique<ClassDescriptor>();
                desc->class_name = cls->name;
                std::vector<semantic::ClassSymbol*> visited;
                collectClassFields(cls, *desc, visited);
                reg.classes_[cls->name] = std::move(desc);
            }
        }

        if (auto* fn = dynamic_cast<semantic::FunctionSymbol*>(sym.get())) {
            if (!fn->captured.empty() || fn->captured_this != nullptr) {
                ClosureCaptureInfo info;
                info.function_name = fn->name;
                for (semantic::Symbol* cap : fn->captured) {
                    if (cap != nullptr) {
                        info.captured_names.push_back(cap->name);
                    }
                }
                if (fn->captured_this != nullptr) {
                    info.captures_this = true;
                    info.this_class_name = fn->captured_this->name;
                }
                reg.closures_.push_back(std::move(info));
            }
        }
    }

    for (const auto& child : scope->children()) {
        walkScope(child.get(), reg);
    }
}

DescriptorRegistry DescriptorBuilder::build(const semantic::SymbolTable& table) {
    DescriptorRegistry reg;
    walkScope(table.global(), reg);
    return reg;
}

}  // namespace runtime
}  // namespace compiscript
