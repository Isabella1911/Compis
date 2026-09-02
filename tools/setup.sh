#!/bin/bash
# Instala el toolchain de ANTLR (JRE + antlr-complete.jar + cmake portatil +
# runtime C++ compilado) sin necesitar sudo ni privilegios de administrador.
# Todo queda dentro de compiscript/.deps/, gitignoreado.
#
# Uso: bash tools/setup.sh   (desde la carpeta compiscript/, o desde donde sea:
# el script se ubica solo por la ruta de este archivo)
set -e

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS="$HERE/.deps"
mkdir -p "$DEPS"

echo "== JRE 17 (Temurin) =="
if [ ! -x "$DEPS/jre17/bin/java" ]; then
    curl -s "https://api.adoptium.net/v3/assets/latest/17/hotspot?architecture=x64&image_type=jre&os=linux&vendor=eclipse" \
        > "$DEPS/adoptium.json"
    URL=$(python3 -c "import json; print(json.load(open('$DEPS/adoptium.json'))[0]['binary']['package']['link'])")
    curl -sL "$URL" -o "$DEPS/jre17.tar.gz"
    mkdir -p "$DEPS/jre17"
    tar -xzf "$DEPS/jre17.tar.gz" -C "$DEPS/jre17" --strip-components=1
else
    echo "ya instalado"
fi
"$DEPS/jre17/bin/java" -version

echo "== ANTLR 4.13.2 complete.jar =="
if [ ! -f "$DEPS/antlr-4.13.2-complete.jar" ]; then
    curl -sL https://www.antlr.org/download/antlr-4.13.2-complete.jar \
        -o "$DEPS/antlr-4.13.2-complete.jar"
else
    echo "ya instalado"
fi

echo "== cmake portatil (para compilar el runtime C++ de ANTLR una sola vez) =="
if [ ! -x "$DEPS/cmake/bin/cmake" ]; then
    curl -sL https://github.com/Kitware/CMake/releases/download/v3.30.5/cmake-3.30.5-linux-x86_64.tar.gz \
        -o "$DEPS/cmake.tar.gz"
    mkdir -p "$DEPS/cmake"
    tar -xzf "$DEPS/cmake.tar.gz" -C "$DEPS/cmake" --strip-components=1
else
    echo "ya instalado"
fi

echo "== antlr4-cpp-runtime 4.13.2 (fuente + build) =="
if [ ! -f "$DEPS/antlr4cpp/lib/libantlr4-runtime.a" ]; then
    curl -sL https://www.antlr.org/download/antlr4-cpp-runtime-4.13.2-source.zip \
        -o "$DEPS/antlr4-cpp-runtime-src.zip"
    mkdir -p "$DEPS/antlr4-cpp-runtime-src"
    python3 -c "import zipfile; zipfile.ZipFile('$DEPS/antlr4-cpp-runtime-src.zip').extractall('$DEPS/antlr4-cpp-runtime-src')"
    cd "$DEPS/antlr4-cpp-runtime-src"
    mkdir -p build && cd build
    "$DEPS/cmake/bin/cmake" .. \
        -DCMAKE_INSTALL_PREFIX="$DEPS/antlr4cpp" \
        -DCMAKE_BUILD_TYPE=Release \
        -DANTLR4_INSTALL=ON \
        -DANTLR_BUILD_SHARED=OFF \
        -DANTLR_BUILD_CPP_TESTS=OFF
    "$DEPS/cmake/bin/cmake" --build . -j "$(nproc)"
    "$DEPS/cmake/bin/cmake" --install .
else
    echo "ya instalado"
fi

echo
echo "Listo. make ya encuentra todo esto automaticamente via .deps/."
