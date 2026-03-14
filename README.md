# Compis
**paso 1**
g++ -std=c++17 -o yalex_parser YalexParser.cpp -O2
esto compila YalexParser.cpp

**paso 2**
./yalex_parser lexer_complejo.yal -o lexer_compl
generar un analizador léxico --Lexer simple

./yalex_parser lexer_complejo.yal -o lexer_complejo
--Lexer complejo

**paso 3**
g++ -std=c++17 -o mi_lexer mi_lexer.cpp -O2
--simple

o 

g++ -std=c++17 -o lexer_complejo lexer_complejo.cpp -O2
--complejo

Compilar el analizador léxico generado

**paso 4**
./mi_lexer test_input.txt
-- simple

./lexer_complejo test_complejo.txt
--complejo

Ejecutar el lexer sobre un archivo de texto

**paso 5**
pip install graphviz
python3 visualizar.py .