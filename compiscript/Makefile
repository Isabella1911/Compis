# Compiscript (Proyecto 2) — build del frontend ANTLR + AST + diagnosticos.
#
# Primera vez en una maquina nueva:
#   bash tools/setup.sh     # instala JRE, antlr.jar, cmake y compila el runtime C++ de ANTLR
#                            # todo local a .deps/, sin sudo
#
# Uso normal:
#   make              -> genera el parser (si hace falta) y compila compiscript(.exe)
#   make run FILE=tests/fixtures/valid/hello.cps
#   make test         -> corre todos los fixtures de tests/fixtures/valid e invalid
#   make test-symbols -> corre la prueba unitaria de Scope/Symbol (no necesita ANTLR)
#   make clean        -> borra los binarios y el codigo generado por ANTLR

DEPS        := .deps
JAVA        := $(DEPS)/jre17/bin/java
ANTLR_JAR   := $(DEPS)/antlr-4.13.2-complete.jar
ANTLR4CPP   := $(DEPS)/antlr4cpp

CXX        := g++
CXXFLAGS   := -std=c++17 -O0 -g -Wall \
              -I $(ANTLR4CPP)/include/antlr4-runtime \
              -I generated \
              -I src
LDFLAGS    := $(ANTLR4CPP)/lib/libantlr4-runtime.a

GRAMMAR    := grammar/Compiscript.g4
GENERATED  := generated/CompiscriptLexer.cpp generated/CompiscriptParser.cpp \
              generated/CompiscriptBaseVisitor.cpp generated/CompiscriptVisitor.cpp

SRC        := src/main.cpp \
              src/ast/printer.cpp \
              src/diagnostics/reporter.cpp \
              src/frontend/error_listener.cpp \
              src/frontend/ast_builder.cpp \
              src/frontend/parser_driver.cpp \
              src/compiler/compiler.cpp \
              $(GENERATED)

SEMANTIC_SRC := src/semantic/scope.cpp

UNAME_S := $(shell uname -s 2>/dev/null)
ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
BIN             := compiscript.exe
TEST_SYMBOLS_BIN := symbol_table_test.exe
else
BIN             := compiscript
TEST_SYMBOLS_BIN := symbol_table_test
endif

.PHONY: all build setup generate test test-symbols clean

all: build

setup:
	bash tools/setup.sh

# El .g4 es la unica dependencia real: si no cambio, no se regenera.
$(GENERATED) &: $(GRAMMAR)
	$(JAVA) -jar $(ANTLR_JAR) -Dlanguage=Cpp -visitor -no-listener -package compiscript \
	    -o generated -Xexact-output-dir $(GRAMMAR)

generate: $(GENERATED)

build: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(LDFLAGS) -o $(BIN)

run: build
	./$(BIN) $(FILE)

test: build
	@ok=0; fail=0; \
	for f in tests/fixtures/valid/*.cps; do \
	    [ -e "$$f" ] || continue; \
	    if ./$(BIN) "$$f" > /dev/null 2>&1; then \
	        echo "OK      $$f"; ok=$$((ok+1)); \
	    else \
	        echo "FALLO   $$f (se esperaba que compilara sin errores)"; fail=$$((fail+1)); \
	    fi; \
	done; \
	for f in tests/fixtures/invalid/*.cps; do \
	    [ -e "$$f" ] || continue; \
	    if ./$(BIN) "$$f" > /dev/null 2>&1; then \
	        echo "FALLO   $$f (se esperaba un error y acepto)"; fail=$$((fail+1)); \
	    else \
	        echo "OK      $$f"; ok=$$((ok+1)); \
	    fi; \
	done; \
	echo "----"; echo "$$ok ok, $$fail fallos"; \
	[ "$$fail" -eq 0 ]

# No depende de $(GENERATED) ni de ANTLR4CPP a proposito: semantic/ no
# incluye nada de ANTLR, asi que esta prueba compila (mucho) mas rapido.
$(TEST_SYMBOLS_BIN): tests/symbol_table_test.cpp $(SEMANTIC_SRC)
	$(CXX) -std=c++17 -O0 -g -Wall -I src tests/symbol_table_test.cpp $(SEMANTIC_SRC) -o $(TEST_SYMBOLS_BIN)

test-symbols: $(TEST_SYMBOLS_BIN)
	./$(TEST_SYMBOLS_BIN)

clean:
	rm -f compiscript compiscript.exe symbol_table_test symbol_table_test.exe
	rm -rf generated
