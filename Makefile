# sqlint: interprete SQL con almacenamiento en archivos (docs/sql).
#
# Primera vez en una maquina nueva:
#   bash tools/setup.sh     # instala JRE, antlr.jar, cmake y compila el runtime C++ de ANTLR
#                           # todo local a .deps/, sin sudo
#
# Uso normal:
#   make                    -> genera el parser (si hace falta) y compila sqlint
#   make run FILE=consulta.sql [DB=databases/escuela]
#   make test               -> bateria de pruebas (runner de la Parte 2)
#   make test-parte1        -> casos lexicos, sintacticos y semanticos (Parte 1)
#   make test-update        -> regenera los .expected (revisar el diff!)
#   make test-storage       -> pruebas unitarias de la Parte 2 (sin ANTLR)
#   make test-mutator       -> Mutator con scripts reales y databases/escuela == crear.sql
#   make clean              -> borra build/, generated/ y el binario
#
# Los fuentes se toman con wildcard (src/*.cpp y src/*/*.cpp): un archivo
# nuevo de cualquier parte entra solo, nadie tiene que tocar este Makefile.
# Cada .cpp compila a su propio .o en build/, asi que editar un archivo no
# recompila el parser generado (que es lo lento).

DEPS        := .deps
JAVA        := $(DEPS)/jre17/bin/java
ANTLR_JAR   := $(DEPS)/antlr-4.13.2-complete.jar
ANTLR4CPP   := $(DEPS)/antlr4cpp

CXX         := g++
# El runtime de ANTLR se incluye con -isystem: sus warnings no son nuestros
# y taparian los de src/. generated/ NO: -MMD no registra los headers de
# sistema como dependencias, y si la gramatica cambia los numeros de token
# hay que recompilar todo lo que incluye SQLParser.h.
CXXFLAGS    := -std=c++17 -O0 -g -Wall \
               -isystem $(ANTLR4CPP)/include/antlr4-runtime \
               -I generated \
               -I src
GEN_CXXFLAGS := $(CXXFLAGS) -w
LDFLAGS     := $(ANTLR4CPP)/lib/libantlr4-runtime.a

GRAMMAR     := grammar/SQL.g4
GENERATED   := generated/SQLLexer.cpp generated/SQLParser.cpp \
               generated/SQLBaseVisitor.cpp generated/SQLVisitor.cpp

OBJDIR      := build
SRC         := $(wildcard src/*.cpp src/*/*.cpp)
OBJ         := $(SRC:%.cpp=$(OBJDIR)/%.o)
GEN_OBJ     := $(GENERATED:%.cpp=$(OBJDIR)/%.o)

BIN         := sqlint
DB          ?= databases/escuela
PYTHON      ?= python3

.PHONY: all build setup generate run test test-parte1 test-update test-storage test-mutator test-exec clean

all: build

setup:
	bash tools/setup.sh

# El .g4 es la unica dependencia real: si no cambio, no se regenera.
$(GENERATED) &: $(GRAMMAR)
	$(JAVA) -jar $(ANTLR_JAR) -Dlanguage=Cpp -visitor -no-listener -package sqlint \
	    -o generated -Xexact-output-dir $(GRAMMAR)

generate: $(GENERATED)

build: $(BIN)

$(BIN): $(OBJ) $(GEN_OBJ)
	$(CXX) $^ $(LDFLAGS) -o $@

# Codigo propio. Orden: los headers generados tienen que existir antes de
# compilar frontend/ (por eso el prerequisito "| $(GENERATED)").
$(OBJDIR)/src/%.o: src/%.cpp | $(GENERATED)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/generated/%.o: generated/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(GEN_CXXFLAGS) -MMD -MP -c $< -o $@

# Dependencias de headers que escribe -MMD: cambiar un .h recompila solo
# los .cpp que lo incluyen.
-include $(OBJ:.o=.d) $(GEN_OBJ:.o=.d)

run: build
	./$(BIN) --db $(DB) $(FILE)

# El runner lo escribe la Parte 2 (tests/sql/run_tests.py).
test: build test-parte1 test-storage test-mutator test-exec
	@if [ -f tests/sql/run_tests.py ]; then \
	    $(PYTHON) tests/sql/run_tests.py --bin ./$(BIN); \
	else \
	    echo "Todavia no hay tests/sql/run_tests.py (Parte 2)."; \
	fi

# Casos de la Parte 1 (lexico, sintactico, semantico): codigo y posicion de
# cada error, sin necesitar base de datos ni ejecucion.
test-parte1: build
	@$(PYTHON) tests/sql/verificar_parte1.py --bin ./$(BIN)

test-update: build
	@$(PYTHON) tests/sql/run_tests.py --bin ./$(BIN) --update
	@echo "Revisa los cambios con: git diff tests/sql"

# Pruebas unitarias de la Parte 2: valores, CSV, catalogo, TableStore e
# indices. No usan el parser, asi que compilan sin ANTLR (como el
# test-symbols de Compiscript).
STORAGE_SRC := src/storage/value.cpp src/storage/csv.cpp src/storage/fs_util.cpp \
               src/storage/catalog.cpp src/storage/table_store.cpp \
               src/index/index_file.cpp src/index/index_manager.cpp

test-storage:
	@mkdir -p $(OBJDIR)
	$(CXX) -std=c++17 -O0 -g -Wall -I src $(STORAGE_SRC) tests/sql/storage/test_storage.cpp \
	    -o $(OBJDIR)/test_storage
	@./$(OBJDIR)/test_storage

# Mutator con scripts parseados y analizados de verdad. Enlaza todo menos
# main.o. Tambien revisa que databases/escuela sea lo que produce crear.sql.
$(OBJDIR)/test_mutator: tests/sql/storage/test_mutator.cpp $(filter-out $(OBJDIR)/src/main.o,$(OBJ)) $(GEN_OBJ)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

test-mutator: $(OBJDIR)/test_mutator
	@./$(OBJDIR)/test_mutator

# Parte 3: contratos de binding/cache y oraculos de ejecucion de punta a punta.
$(OBJDIR)/test_exec: tests/sql/exec/test_exec.cpp $(filter-out $(OBJDIR)/src/main.o,$(OBJ)) $(GEN_OBJ)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

test-exec: build $(OBJDIR)/test_exec
	@./$(OBJDIR)/test_exec
	@$(PYTHON) tests/sql/verificar_ejecucion.py --bin ./$(BIN)

clean:
	rm -rf $(OBJDIR) generated $(BIN)
