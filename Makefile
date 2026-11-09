# Compiscript (Proyecto 2) — build del frontend ANTLR + AST + diagnosticos.
#
# Primera vez en una maquina nueva:
#   bash tools/setup.sh     # instala JRE, antlr.jar, cmake y compila el runtime C++ de ANTLR
#                            # todo local a .deps/, sin sudo
#
# Uso normal:
#   make              -> genera el parser (si hace falta) y compila compiscript(.exe)
#   make run FILE=tests/fixtures/valid/hello.cps
#   make test         -> corre toda la bateria: tabla de simbolos, semantica y TAC
#   make test-semantic / make test-tac -> solo una parte
#   make test-tac-update -> regenera los .tac esperados (revisar el diff!)
#   make test-symbols -> corre la prueba unitaria de Scope/Symbol (no necesita ANTLR)
#   make test-mips    -> backend MIPS: integracion (CLI) + extremo a extremo en
#                        MARS. Sin MARS, la parte e2e queda en SKIP (desarrollo)
#   make test-mips-integration / test-mips-e2e -> solo una de las dos
#   make test-mips-unit -> pruebas unitarias del backend (TAC armado a mano)
#   make test-cli / test-ide -> CLI publica y la IDE (esta ultima necesita display)
#   make test-mips-estricto -> validacion de entrega: exige MARS (sin SKIP)
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

SEMANTIC_SRC := src/semantic/scope.cpp \
                src/semantic/type.cpp \
                src/semantic/declaration_collector.cpp \
                src/semantic/inheritance_resolver.cpp \
                src/semantic/name_resolver.cpp \
                src/semantic/type_checker.cpp \
                src/semantic/control_flow_checker.cpp \
                src/semantic/closure_analyzer.cpp \
                src/semantic/printer.cpp
# Generacion de codigo intermedio (Proyecto 2). Todos los archivos ya
# estan listados, incluidos los de cada parte del reparto, para que nadie
# tenga que tocar el Makefile al implementar su parte.
CODEGEN_SRC := src/ir/tac.cpp \
               src/ir/tac_printer.cpp \
               src/codegen/temp_allocator.cpp \
               src/codegen/frame_layout.cpp \
               src/codegen/tac_generator.cpp \
               src/codegen/gen_expressions.cpp \
               src/codegen/gen_statements.cpp \
               src/codegen/gen_functions.cpp \
               src/codegen/gen_classes.cpp \
               src/codegen/gen_arrays.cpp \
               src/codegen/mips/mips_emitter.cpp \
               src/codegen/mips/mips_generator.cpp
# Subconjunto que necesita la prueba unitaria de la tabla de simbolos: no
# arrastra declaration_collector.cpp (depende del AST) ni printer.cpp.
SYMBOL_TEST_SRC := src/semantic/scope.cpp
# Pruebas unitarias del backend MIPS: solo el IR y el generador, sin ANTLR.
MIPS_UNIT_SRC := src/ir/tac.cpp src/codegen/mips/mips_emitter.cpp                  src/codegen/mips/mips_generator.cpp
MIPS_UNIT_BIN := mips_backend_test

SRC        := src/main.cpp \
              src/ast/printer.cpp \
              src/diagnostics/reporter.cpp \
              src/frontend/error_listener.cpp \
              src/frontend/ast_builder.cpp \
              src/frontend/parser_driver.cpp \
              src/compiler/compiler.cpp \
              $(SEMANTIC_SRC) \
              $(CODEGEN_SRC) \
              $(GENERATED)

# Los headers tambien son dependencia: sin esto, cambiar un .h (p. ej.
# symbol.h o tac.h) no recompilaba nada.
HEADERS    := $(wildcard src/*/*.h src/*/*/*.h)

UNAME_S := $(shell uname -s 2>/dev/null)
ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
BIN             := compiscript.exe
TEST_SYMBOLS_BIN := symbol_table_test.exe
else
BIN             := compiscript
TEST_SYMBOLS_BIN := symbol_table_test
endif

.PHONY: all build setup generate run test test-cli test-ide test-semantic test-tac test-tac-update docs-ejemplos test-symbols test-mips test-mips-runner test-mips-unit test-mips-integration test-mips-e2e test-mips-estricto clean

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

# La bateria vive en tests/run_tests.py (Python 3, sin dependencias):
#   - tests/fixtures/valid y invalid: compila / falla con los codigos exactos
#     de sus lineas '// expect: SEMxxx'
#   - tests/tac: la salida de --tac contra los .tac esperados (golden files),
#     y tests/tac/errores: sin TAC y con los codigos esperados
PYTHON ?= python3

# Corre TODAS las suites aunque alguna falle (asi el resumen de cada una
# queda completo, tambien en F7 de la IDE) y termina con error si fallo
# cualquiera. Las suites que necesitan MARS quedan en SKIP si no esta.
test: build $(TEST_SYMBOLS_BIN) $(MIPS_UNIT_BIN)
	@st=0; 	echo "== Prueba unitaria de la tabla de simbolos =="; ./$(TEST_SYMBOLS_BIN) || st=1; 	$(PYTHON) tests/run_tests.py --bin ./$(BIN) todo || st=1; 	echo "== Runner de MARS =="; $(PYTHON) tests/mips/test_mars_runner.py || st=1; 	echo "== Unitarias del backend MIPS =="; $(PYTHON) tests/mips/unit/run_unit.py --bin ./$(MIPS_UNIT_BIN) || st=1; 	$(PYTHON) tests/mips/run_mips_tests.py --bin ./$(BIN) --suite todo || st=1; 	echo "== CLI =="; $(PYTHON) tests/cli/test_cli.py --bin ./$(BIN) || st=1; 	exit $$st

test-semantic: build
	@$(PYTHON) tests/run_tests.py --bin ./$(BIN) semantica

test-tac: build
	@$(PYTHON) tests/run_tests.py --bin ./$(BIN) tac

# Regenera los .tac esperados. Revisar SIEMPRE `git diff tests/tac` antes de
# commitear: si no, un bug se vuelve "lo esperado".
test-tac-update: build
	@$(PYTHON) tests/run_tests.py --bin ./$(BIN) tac --update
	@echo "Revisa los cambios con: git diff tests/tac"

# Regenera docs/10_ejemplos_de_traduccion.md a partir de tests/tac/.
docs-ejemplos:
	$(PYTHON) tools/gen_ejemplos_doc.py

# No depende de $(GENERATED) ni de ANTLR4CPP a proposito: semantic/ no
# incluye nada de ANTLR, asi que esta prueba compila (mucho) mas rapido.
$(TEST_SYMBOLS_BIN): tests/symbol_table_test.cpp $(SYMBOL_TEST_SRC)
	$(CXX) -std=c++17 -O0 -g -Wall -I src tests/symbol_table_test.cpp $(SYMBOL_TEST_SRC) -o $(TEST_SYMBOLS_BIN)

test-symbols: $(TEST_SYMBOLS_BIN)
	./$(TEST_SYMBOLS_BIN)

# Backend MIPS (tests/mips/run_mips_tests.py, MARS en tests/mips/mars.py).
# Los resultados de cada corrida quedan en tests/results/mips_<suite>.json.
test-mips: build test-mips-runner test-mips-unit
	@$(PYTHON) tests/mips/run_mips_tests.py --bin ./$(BIN) --suite todo

$(MIPS_UNIT_BIN): tests/mips/unit/test_mips_backend.cpp $(MIPS_UNIT_SRC) $(HEADERS)
	$(CXX) -std=c++17 -O0 -g -Wall -I src tests/mips/unit/test_mips_backend.cpp $(MIPS_UNIT_SRC) -o $(MIPS_UNIT_BIN)

test-mips-unit: $(MIPS_UNIT_BIN)
	@$(PYTHON) tests/mips/unit/run_unit.py --bin ./$(MIPS_UNIT_BIN)

# Clasificador de tests/mips/mars.py contra MARS real (programas a mano que
# terminan de cada forma posible). Sin MARS: skip con el motivo.
test-mips-runner:
	@$(PYTHON) tests/mips/test_mars_runner.py

test-mips-integration: build
	@$(PYTHON) tests/mips/run_mips_tests.py --bin ./$(BIN) --suite integracion

test-mips-e2e: build
	@$(PYTHON) tests/mips/run_mips_tests.py --bin ./$(BIN) --suite e2e

# CLI publica: rutas inexistentes, archivo vacio, tildes y espacios, salida
# en un directorio que no existe, flags incompatibles, entrada grande.
test-cli: build
	@$(PYTHON) tests/cli/test_cli.py --bin ./$(BIN)

# IDE (Tkinter): necesita un display (WSLg o X11); sin el queda en SKIP.
test-ide: build
	@$(PYTHON) tests/ide/test_ide.py

test-mips-estricto: build $(MIPS_UNIT_BIN)
	@COMPISCRIPT_ESTRICTO=1 $(PYTHON) tests/mips/test_mars_runner.py
	@COMPISCRIPT_ESTRICTO=1 $(PYTHON) tests/mips/unit/run_unit.py --bin ./$(MIPS_UNIT_BIN)
	@COMPISCRIPT_ESTRICTO=1 $(PYTHON) tests/cli/test_cli.py --bin ./$(BIN)
	@COMPISCRIPT_ESTRICTO=1 $(PYTHON) tests/ide/test_ide.py
	@$(PYTHON) tests/mips/run_mips_tests.py --bin ./$(BIN) --suite todo --estricto

clean:
	rm -f compiscript compiscript.exe symbol_table_test symbol_table_test.exe mips_backend_test
	rm -rf generated
