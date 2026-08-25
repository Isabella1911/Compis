# Compis — build y ejecucion del back en un solo comando.
# Uso:
#   make                 -> compila compilador(.exe)
#   make ejemplo         -> compila (si hace falta) y corre el set input/ejemplo/
#   make complejo        -> compila (si hace falta) y corre el set input/complejo/
#   make menu            -> corre el modo interactivo (sin argumentos)
#   make run YAL=... YAPAR=... IN=...   -> corre un set de archivos a mano
#   make clean           -> borra el binario compilado

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -DCOMPILAR_CON_ORQUESTADOR

SRC := Main.cpp \
       lexer/YalexParser.cpp \
       parser/YaparParser.cpp parser/Grammar.cpp \
       parser/FirstFollow.cpp parser/LL1Table.cpp \
       parser/LR0.cpp parser/SLR1.cpp parser/LALR1.cpp

UNAME_S := $(shell uname -s 2>/dev/null)
ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S)))
BIN := compilador.exe
else
BIN := compilador
endif

.PHONY: all build menu ejemplo complejo run clean

all: build

build: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

menu: build
	./$(BIN)

ejemplo: build
	./$(BIN) input/ejemplo/ejemplo.yal input/ejemplo/parser_ejemplo.yapar input/ejemplo/entrada_ejemplo.txt

complejo: build
	./$(BIN) input/complejo/lexer_complejo.yal input/complejo/parser_complejo.yapar input/complejo/entrada_complejo.txt

run: build
	./$(BIN) $(YAL) $(YAPAR) $(IN)

clean:
	rm -f compilador compilador.exe
