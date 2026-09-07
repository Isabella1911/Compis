// Pruebas del runtime/GC sin ANTLR: heap, mark-and-sweep (incl. ciclos),
// y descriptores derivados de ClassSymbol / FunctionSymbol::captured.

#include <cassert>
#include <iostream>
#include <string>

#include "runtime/descriptor_builder.h"
#include "runtime/gc.h"
#include "runtime/heap.h"
#include "semantic/scope.h"
#include "semantic/symbol.h"
#include "semantic/symbol_table.h"
#include "semantic/type.h"

using namespace compiscript::runtime;
using namespace compiscript::semantic;

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks++;                                                       \
        if (!(cond)) {                                                  \
            failures++;                                                 \
            std::cerr << "FALLO linea " << __LINE__ << ": " << #cond << "\n"; \
        }                                                                \
    } while (0)

static SymbolPtr makeField(const std::string& name, TypePtr type) {
    auto s = std::make_shared<Symbol>();
    s->name = name;
    s->kind = SymbolKind::Variable;
    s->resolved_type = std::move(type);
    return s;
}

static void testMarkSweepCycle() {
    ClassDescriptor nodeDesc;
    nodeDesc.class_name = "Node";
    nodeDesc.fields.push_back(FieldSlot{"value", SlotKind::Primitive, 0});
    nodeDesc.fields.push_back(FieldSlot{"next", SlotKind::Pointer, 1});

    Heap heap;
    GarbageCollector gc(heap, /*threshold*/ 1000);

    HeapObject* a = gc.allocClass(&nodeDesc);
    HeapObject* b = gc.allocClass(&nodeDesc);
    Heap::setWord(a, 0, 1);
    Heap::setWord(b, 0, 2);
    Heap::setPointer(a, 1, b);
    Heap::setPointer(b, 1, a);  // ciclo A <-> B

    HeapObject* orphan = gc.allocClass(&nodeDesc);
    Heap::setWord(orphan, 0, 99);

    CHECK(heap.liveCount() == 3);

    // Solo A es raiz: B se alcanza por el ciclo; orphan no.
    gc.clearRoots();
    gc.addRoot(a);
    std::size_t freed = gc.collect();
    CHECK(freed == 1);
    CHECK(heap.liveCount() == 2);

    // Sin raices: se libera el ciclo completo.
    gc.clearRoots();
    freed = gc.collect();
    CHECK(freed == 2);
    CHECK(heap.liveCount() == 0);
}

static void testArrayOfPointers() {
    ClassDescriptor box;
    box.class_name = "Box";
    box.fields.push_back(FieldSlot{"x", SlotKind::Primitive, 0});

    ArrayDescriptor arrDesc;
    arrDesc.element_kind = SlotKind::Pointer;

    Heap heap;
    GarbageCollector gc(heap, 1000);

    HeapObject* b1 = gc.allocClass(&box);
    HeapObject* b2 = gc.allocClass(&box);
    HeapObject* arr = gc.allocArray(&arrDesc, 2);
    Heap::setPointer(arr, 0, b1);
    Heap::setPointer(arr, 1, b2);

    HeapObject* lost = gc.allocClass(&box);

    gc.clearRoots();
    gc.addRoot(arr);
    CHECK(gc.collect() == 1);
    CHECK(heap.liveCount() == 3);
    (void)lost;
}

static void testThresholdTriggersCollect() {
    ClassDescriptor empty;
    empty.class_name = "E";

    Heap heap;
    GarbageCollector gc(heap, /*threshold*/ 2);

    HeapObject* keep = gc.allocClass(&empty);
    gc.addRoot(keep);
    (void)gc.allocClass(&empty);  // live=2, aun no dispara (umbral al inicio del alloc)
    (void)gc.allocClass(&empty);  // live>=2 -> collect; keep sobrevive, el otro no es raiz

    // Tras el tercer alloc: collect dejo solo keep, luego alloc del tercero.
    CHECK(heap.liveCount() == 2);  // keep + el recien allocado
    gc.clearRoots();
    gc.addRoot(keep);
    CHECK(gc.collect() == 1);
    CHECK(heap.liveCount() == 1);
}

static void testDescriptorBuilderFromSymbols() {
    SymbolTable table;
    Scope* global = table.global();

    auto animal = std::make_shared<ClassSymbol>();
    animal->name = "Animal";
    animal->kind = SymbolKind::Class;
    animal->class_scope = global->createChild(ScopeKind::Class);
    animal->class_scope->owner = animal.get();
    animal->class_scope->declare(makeField("nombre", makeStringType()));
    global->declare(animal);

    auto perro = std::make_shared<ClassSymbol>();
    perro->name = "Perro";
    perro->kind = SymbolKind::Class;
    perro->base_class = animal.get();
    perro->class_scope = global->createChild(ScopeKind::Class);
    perro->class_scope->owner = perro.get();
    perro->class_scope->declare(makeField("edad", makeIntegerType()));
    perro->class_scope->declare(makeField("amigo", makeClassType(animal.get())));
    global->declare(perro);

    // Closure anidada que captura un parametro.
    auto outer = std::make_shared<FunctionSymbol>();
    outer->name = "outer";
    outer->kind = SymbolKind::Function;
    outer->function_scope = global->createChild(ScopeKind::Function);
    outer->function_scope->owner = outer.get();
    auto param = std::make_shared<Symbol>();
    param->name = "x";
    param->kind = SymbolKind::Parameter;
    param->resolved_type = makeIntegerType();
    outer->function_scope->declare(param);
    global->declare(outer);

    auto inner = std::make_shared<FunctionSymbol>();
    inner->name = "inner";
    inner->kind = SymbolKind::Function;
    inner->function_scope = outer->function_scope->createChild(ScopeKind::Function);
    inner->function_scope->owner = inner.get();
    inner->captured.push_back(param.get());
    outer->function_scope->declare(inner);

    DescriptorRegistry reg = DescriptorBuilder::build(table);

    const ClassDescriptor* animalDesc = reg.findClass("Animal");
    const ClassDescriptor* perroDesc = reg.findClass("Perro");
    CHECK(animalDesc != nullptr);
    CHECK(perroDesc != nullptr);
    CHECK(animalDesc->fields.size() == 1);
    CHECK(animalDesc->fields[0].name == "nombre");
    CHECK(animalDesc->fields[0].kind == SlotKind::Primitive);

    // Base primero, luego campos propios ordenados: amigo, edad.
    CHECK(perroDesc->fields.size() == 3);
    CHECK(perroDesc->fields[0].name == "nombre");
    CHECK(perroDesc->fields[1].name == "amigo");
    CHECK(perroDesc->fields[1].kind == SlotKind::Pointer);
    CHECK(perroDesc->fields[2].name == "edad");
    CHECK(perroDesc->fields[2].kind == SlotKind::Primitive);

    CHECK(reg.closures().size() == 1);
    CHECK(reg.closures()[0].function_name == "inner");
    CHECK(reg.closures()[0].captured_names.size() == 1);
    CHECK(reg.closures()[0].captured_names[0] == "x");
    CHECK(reg.closures()[0].captures_this == false);

    const ArrayDescriptor* ptrArr = reg.arrayOf(SlotKind::Pointer);
    const ArrayDescriptor* primArr = reg.arrayOf(SlotKind::Primitive);
    CHECK(ptrArr != nullptr && primArr != nullptr);
    CHECK(ptrArr->element_kind == SlotKind::Pointer);
    CHECK(primArr->element_kind == SlotKind::Primitive);
}

int main() {
    testMarkSweepCycle();
    testArrayOfPointers();
    testThresholdTriggersCollect();
    testDescriptorBuilderFromSymbols();

    std::cout << checks << " checks, " << failures << " fallos\n";
    return failures == 0 ? 0 : 1;
}
