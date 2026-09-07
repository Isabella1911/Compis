# Compiscript (Proyecto 2) — build del frontend ANTLR + AST + diagnosticos.
#
# Primera vez en una maquina nueva:
#   bash tools/setup.sh     # instala JRE, antlr.jar, cmake y compila el runtime C++ de ANTLR
#                            # todo local a .deps/, sin sudo
#
# Uso normal:
#   make              -> genera el parser (si hace falta) y compila compiscript(.exe)
#   make run FILE=tests/fixtures/valid/hello.cps
#   make test         -> corre fixtures + unitarios (symbols, semantic, runtime/GC)
#   make test-symbols -> Scope/Symbol (sin ANTLR)
#   make test-runtime -> heap + mark-and-sweep + descriptores (sin ANTLR)
#   make clean        -> borra los binarios y el codigo generado por ANTLR

DEPS        := .deps
JAVA        := $(DEPS)/jre17/bin/java
ANTLR_JAR   := $(DEPS)/antlr-4.13.2-complete.jar
ANTLR4CPP   := $(DEPS)/antlr4cpp

CXX        := g++
CXXFLAGS   := -std=c++17 -O0 -g -Wall \
              -isystem $(ANTLR4CPP)/include/antlr4-runtime \
              -I generated \
              -I src
LDFLAGS    := $(ANTLR4CPP)/lib/libantlr4-runtime.a

HEADERS     := $(wildcard src/*/*.h)

GRAMMAR    := grammar/Compiscript.g4
GENERATED  := generated/CompiscriptLexer.cpp generated/CompiscriptParser.cpp \
              generated/CompiscriptBaseVisitor.cpp generated/CompiscriptVisitor.cpp

SEMANTIC_SRC := src/semantic/scope.cpp \
                src/semantic/type.cpp \
                src/semantic/declaration_collector.cpp \
                src/semantic/inheritance_resolver.cpp \
                src/semantic/name_resolver.cpp \
                src/semantic/type_checker.cpp \
                src/semantic/control_flow_checker.cpp \
                src/semantic/closure_analyzer.cpp \
                src/semantic/printer.cpp

RUNTIME_SRC := src/runtime/heap.cpp \
               src/runtime/gc.cpp \
               src/runtime/descriptor_builder.cpp

# Subconjunto que necesita la prueba unitaria de la tabla de simbolos: no
# arrastra declaration_collector.cpp (depende del AST) ni printer.cpp.
SYMBOL_TEST_SRC := src/semantic/scope.cpp

SRC        := src/main.cpp \
              src/ast/printer.cpp \
              src/diagnostics/reporter.cpp \
              src/frontend/error_listener.cpp \
              src/frontend/ast_builder.cpp \
              src/frontend/parser_driver.cpp \
              src/compiler/compiler.cpp \
              $(SEMANTIC_SRC) \
              $(GENERATED)

UNAME_S := $(shell uname -s 2>/dev/null)
ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
BIN             := compiscript.exe
TEST_SYMBOLS_BIN := symbol_table_test.exe
TEST_RUNTIME_BIN := runtime_gc_test.exe
else
BIN             := compiscript
TEST_SYMBOLS_BIN := symbol_table_test
TEST_RUNTIME_BIN := runtime_gc_test
endif

.PHONY: all build setup generate run test test-symbols test-semantic test-runtime clean

all: build

setup:
	bash tools/setup.sh

# El .g4 es la unica dependencia real: si no cambio, no se regenera.
$(GENERATED) &: $(GRAMMAR)
	$(JAVA) -jar $(ANTLR_JAR) -Dlanguage=Cpp -visitor -no-listener -package compiscript \
	    -o generated -Xexact-output-dir $(GRAMMAR)

generate: $(GENERATED)

build: $(BIN)

$(BIN): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)

run: build
	./$(BIN) $(FILE)

test: build test-symbols test-semantic test-runtime
	python3 tests/fixture_runner_test.py
	python3 tests/run_fixtures.py ./$(BIN)

# No depende de $(GENERATED) ni de ANTLR4CPP a proposito: semantic/ no
# incluye nada de ANTLR, asi que esta prueba compila (mucho) mas rapido.
$(TEST_SYMBOLS_BIN): tests/symbol_table_test.cpp $(SYMBOL_TEST_SRC) $(HEADERS)
	$(CXX) -std=c++17 -O0 -g -Wall -I src tests/symbol_table_test.cpp $(SYMBOL_TEST_SRC) -o $(TEST_SYMBOLS_BIN)

test-symbols: $(TEST_SYMBOLS_BIN)
	./$(TEST_SYMBOLS_BIN)

# Regresiones de metadatos y recuperacion, usando AST propio sin depender de ANTLR.
output/semantic_test: tests/semantic_test.cpp $(SEMANTIC_SRC) src/diagnostics/reporter.cpp $(HEADERS)
	mkdir -p output
	$(CXX) -std=c++17 -O0 -g -Wall -I src tests/semantic_test.cpp $(SEMANTIC_SRC) src/diagnostics/reporter.cpp -o $@

test-semantic: output/semantic_test
	python3 -c 'import subprocess; subprocess.run(["./output/semantic_test"], check=True, timeout=20)'

# Runtime/GC (fuera de Proyecto 2): heap + mark-and-sweep + descriptores.
# Solo necesita scope/type del semantic layer; no ANTLR ni pases.
$(TEST_RUNTIME_BIN): tests/runtime_gc_test.cpp $(RUNTIME_SRC) \
                     src/semantic/scope.cpp src/semantic/type.cpp \
                     src/diagnostics/reporter.cpp $(HEADERS)
	$(CXX) -std=c++17 -O0 -g -Wall -I src \
	    tests/runtime_gc_test.cpp $(RUNTIME_SRC) \
	    src/semantic/scope.cpp src/semantic/type.cpp \
	    src/diagnostics/reporter.cpp \
	    -o $(TEST_RUNTIME_BIN)

test-runtime: $(TEST_RUNTIME_BIN)
	./$(TEST_RUNTIME_BIN)

clean:
	rm -f compiscript compiscript.exe symbol_table_test symbol_table_test.exe \
	      runtime_gc_test runtime_gc_test.exe output/semantic_test
	rm -rf generated
