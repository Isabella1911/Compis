# Compis

## Requisito previo (Windows PowerShell)
Agrega MSYS2 al PATH de la sesion para compilar y ejecutar:

`$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"`

## Paso 1
Compilar el generador:

`g++ -std=c++17 -O2 -o yalex_parser.exe YalexParser.cpp`

## Paso 2
Generar analizadores lexicos desde YALex:

Lexer simple:

`./yalex_parser.exe ejemplo.yal -o mi_lexer`

Lexer complejo:

`./yalex_parser.exe lexer_complejo.yal -o lexer_complejo`

## Paso 3
Compilar los analizadores generados:

Simple:

`g++ -std=c++17 -O2 -o mi_lexer.exe mi_lexer.cpp`

Complejo:

`g++ -std=c++17 -O2 -o lexer_complejo.exe lexer_complejo.cpp`

## Paso 4
Ejecutar sobre archivos de prueba:

Simple:

`./mi_lexer.exe test_input.txt`

Complejo:

`./lexer_complejo.exe Test_complejo.txt`

## Paso 5 (visualizacion)
Instalar dependencias de Python:

`pip install graphviz`

Instalar Graphviz (dot.exe) y agregarlo al PATH:
https://graphviz.org/download/

Renderizar salida recomendada (SVG):

`python visualizar.py output --formato svg`

Para incluir tambien AFN en el render:

`python visualizar.py output --formato svg --incluir-afn`
