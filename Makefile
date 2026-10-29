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

.PHONY: all build setup generate run test test-parte1 test-update clean

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
test: build
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

clean:
	rm -rf $(OBJDIR) generated $(BIN)
