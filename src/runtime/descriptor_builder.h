#ifndef COMPISCRIPT_RUNTIME_DESCRIPTOR_BUILDER_H
#define COMPISCRIPT_RUNTIME_DESCRIPTOR_BUILDER_H

// Construye ClassDescriptor / ArrayDescriptor a partir de la tabla de
// simbolos ya poblada por DeclarationCollector + TypeChecker. No modifica
// AST ni scopes: solo lee ClassSymbol::class_scope y Type::kind.

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/type_descriptor.h"
#include "semantic/symbol_table.h"
#include "semantic/type.h"

namespace compiscript {
namespace runtime {

struct ClosureCaptureInfo {
    std::string function_name;
    // Nombres de simbolos capturados (variables/parametros externos).
    std::vector<std::string> captured_names;
    // Si la closure necesita el receptor lexico.
    bool captures_this = false;
    std::string this_class_name;
};

class DescriptorRegistry {
public:
    const ClassDescriptor* findClass(const std::string& name) const;
    const ArrayDescriptor* arrayOf(SlotKind element_kind);

    // Metadatos para que el backend futuro sepa que raices extra aporta
    // cada closure (FunctionSymbol::captured / captured_this).
    const std::vector<ClosureCaptureInfo>& closures() const { return closures_; }

    // Acceso a todos los descriptores de clase (pruebas / depuracion).
    const std::unordered_map<std::string, std::unique_ptr<ClassDescriptor>>&
    classes() const {
        return classes_;
    }

private:
    friend class DescriptorBuilder;
    std::unordered_map<std::string, std::unique_ptr<ClassDescriptor>> classes_;
    std::unordered_map<int, std::unique_ptr<ArrayDescriptor>> arrays_;
    std::vector<ClosureCaptureInfo> closures_;
};

class DescriptorBuilder {
public:
    // Recorre el arbol de scopes y genera descriptores. Las clases sin
    // campos o solo con metodos producen un descriptor vacio (payload 0).
    static DescriptorRegistry build(const semantic::SymbolTable& table);

private:
    static SlotKind slotKindFromType(const semantic::TypePtr& type);
    static void collectClassFields(semantic::ClassSymbol* cls,
                                   ClassDescriptor& out,
                                   std::vector<semantic::ClassSymbol*>& visited);
    static void walkScope(semantic::Scope* scope, DescriptorRegistry& reg);
};

}  // namespace runtime
}  // namespace compiscript

#endif  // COMPISCRIPT_RUNTIME_DESCRIPTOR_BUILDER_H
