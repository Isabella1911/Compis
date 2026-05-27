#include "YaparParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

static std::string leer_archivo(const std::string& ruta) {
    std::ifstream f(ruta);
    if (!f.is_open()) {
        throw std::runtime_error("Error: No se pudo abrir archivo YAPar '" + ruta + "'");
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Verifica si una línea comienza (ignorando espacios) con una palabra clave exacta,
// seguida de espacio, fin de línea, o fin de string.
static bool lineaEmpiezaCon(const std::string& linea, const std::string& keyword) {
    std::string t = trim(linea);
    if (t.size() < keyword.size()) return false;
    if (t.substr(0, keyword.size()) != keyword) return false;
    if (t.size() == keyword.size()) return true;
    return std::isspace((unsigned char)t[keyword.size()]) != 0;
}

std::string eliminarComentariosYapar(const std::string& contenido) {
    std::string limpio;
    bool enComentario = false;

    for (size_t i = 0; i < contenido.size(); i++) {
        if (!enComentario && i + 1 < contenido.size()
            && contenido[i] == '/' && contenido[i + 1] == '*') {
            enComentario = true;
            i++;
            continue;
        }

        if (enComentario && i + 1 < contenido.size()
            && contenido[i] == '*' && contenido[i + 1] == '/') {
            enComentario = false;
            i++;
            continue;
        }

        if (!enComentario) {
            limpio += contenido[i];
        }
    }

    if (enComentario) {
        throw std::runtime_error("Error en YAPar: comentario /* sin cierre */");
    }

    return limpio;
}

void procesarLineaToken(const std::string& linea, YaparSpec& spec) {
    std::istringstream iss(linea);
    std::string palabra;

    iss >> palabra;

    if (palabra != "%token") {
        return;
    }

    std::string token;
    while (iss >> token) {
        spec.tokensDeclarados.insert(token);
    }
}

void procesarLineaIgnore(const std::string& linea, YaparSpec& spec) {
    std::istringstream iss(linea);
    std::string palabra;

    iss >> palabra;

    if (palabra != "IGNORE") {
        return;
    }

    // Verificar que ya haya tokens declarados antes de procesar IGNORE
    if (spec.tokensDeclarados.empty()) {
        throw std::runtime_error(
            "Error en YAPar: se encontró IGNORE antes de declarar tokens con %token"
        );
    }

    std::string token;
    while (iss >> token) {
        if (token == "$") {
            throw std::runtime_error(
                "Error en YAPar: no se puede ignorar el símbolo $"
            );
        }
        if (spec.tokensDeclarados.count(token) == 0) {
            throw std::runtime_error(
                "Error en YAPar: el token " + token
                + " aparece en IGNORE pero no fue declarado con %token"
            );
        }
        spec.tokensIgnorados.insert(token);
    }
}

// ─── Función auxiliar interna: parsea un bloque de producción ya extraído ────
static void procesarBloqueProduccion(
    const std::string& bloque,
    std::vector<Produccion>& producciones)
{
    std::string b = trim(bloque);
    if (b.empty()) return;

    size_t colonPos = b.find(':');
    if (colonPos == std::string::npos) return;

    std::string noTerminal = trim(b.substr(0, colonPos));
    if (noTerminal.empty()) return;

    std::string alternativas = b.substr(colonPos + 1);
    std::istringstream altStream(alternativas);
    std::string linea;
    std::vector<std::string> simbolosActuales;

    while (std::getline(altStream, linea)) {
        linea = trim(linea);
        if (linea.empty()) continue;

        if (linea[0] == '|') {
            if (!simbolosActuales.empty()) {
                producciones.emplace_back(noTerminal, simbolosActuales);
                simbolosActuales.clear();
            }
            linea = trim(linea.substr(1));
        }

        if (!linea.empty()) {
            std::istringstream symStream(linea);
            std::string sym;
            while (symStream >> sym) {
                if (sym != "|") {
                    simbolosActuales.push_back(sym);
                }
            }
        }
    }

    if (!simbolosActuales.empty()) {
        producciones.emplace_back(noTerminal, simbolosActuales);
    }
}

std::vector<Produccion> parsearProducciones(
    const std::string& contenido,
    const YaparSpec& /*spec*/) {

    std::vector<Produccion> producciones;
    std::string produccionCompleta;

    for (char c : contenido) {
        if (c == ';') {
            // Bloque delimitado por ';' — caso normal
            procesarBloqueProduccion(produccionCompleta, producciones);
            produccionCompleta.clear();
        } else {
            produccionCompleta += c;
        }
    }

    // FIX 2: si quedó contenido sin ';' al final del archivo (última producción
    // sin punto y coma), procesarla igual que si hubiera tenido ';'.
    produccionCompleta = trim(produccionCompleta);
    if (!produccionCompleta.empty()) {
        procesarBloqueProduccion(produccionCompleta, producciones);
    }

    return producciones;
}

YaparSpec leerYapar(const std::string& ruta) {
    std::string contenido = leer_archivo(ruta);

    contenido = eliminarComentariosYapar(contenido);

    size_t separador = contenido.find("%%");
    if (separador == std::string::npos) {
        throw std::runtime_error(
            "Error en YAPar: no se encontró el separador %% entre tokens y producciones"
        );
    }

    std::string seccionTokens = contenido.substr(0, separador);
    std::string seccionProducciones = contenido.substr(separador + 2);

    YaparSpec spec;

    std::istringstream tokenStream(seccionTokens);
    std::string linea;
    while (std::getline(tokenStream, linea)) {
        linea = trim(linea);
        if (linea.empty()) continue;

        // Usar comprobación exacta de palabra clave para evitar falsos positivos
        // (ej: un token llamado IGNOREDTOKEN no debe matchear IGNORE)
        if (lineaEmpiezaCon(linea, "%token")) {
            procesarLineaToken(linea, spec);
        } else if (lineaEmpiezaCon(linea, "IGNORE")) {
            procesarLineaIgnore(linea, spec);
        }
    }

    spec.producciones = parsearProducciones(seccionProducciones, spec);

    if (spec.producciones.empty()) {
        throw std::runtime_error("Error en YAPar: no se encontraron producciones");
    }

    spec.simboloInicial = spec.producciones[0].izquierda;

    return spec;
}

ResultadoLexico filtrarTokensIgnorados(
    const ResultadoLexico& entrada,
    const std::set<std::string>& ignorados) {

    ResultadoLexico filtrado;

    for (size_t i = 0; i < entrada.tokens.size(); i++) {
        const Token& token = entrada.tokens[i];

        // El token $ nunca debe eliminarse, aunque esté listado en IGNORE por error
        if (token.id == "$") {
            filtrado.tokens.push_back(token);
            filtrado.posiciones.push_back(entrada.posiciones[i]);
            continue;
        }

        if (ignorados.count(token.id) > 0) {
            continue;
        }

        filtrado.tokens.push_back(token);
        filtrado.posiciones.push_back(entrada.posiciones[i]);
    }

    if (filtrado.tokens.empty() || filtrado.tokens.back().id != "$") {
        filtrado.tokens.push_back({"$", "$"});
        filtrado.posiciones.push_back({-1, -1});
    }

    return filtrado;
}

Gramatica construirGramatica(const YaparSpec& spec) {
    Gramatica g;

    g.terminales = spec.tokensDeclarados;
    g.terminales.insert("$");

    for (const auto& prod : spec.producciones) {
        g.noTerminales.insert(prod.izquierda);
    }

    g.producciones = spec.producciones;
    g.simboloInicial = spec.simboloInicial;

    validarGramatica(g);

    return g;
}

// ── Validaciones (Avance 4) ───────────────────────────────────────────────────

void validarTokensDeEntrada(
    const std::vector<Token>& tokens,
    const std::vector<TokenPosicion>& posiciones,
    const YaparSpec& spec)
{
    for (size_t i = 0; i < tokens.size(); i++) {
        const Token& token = tokens[i];

        if (token.id == "$") continue;

        // FIX 1: tokens con id "lexbuf" son whitespace/comentarios que el lexer
        // emite internamente cuando la acción del .yal es "return lexbuf".
        // El .yapar no los declara (ni necesita hacerlo); se descartan en el
        // filtrado igual que los tokens IGNORE. Omitir el error de validación
        // para no reportar falsos positivos en .yal bien escritos.
        if (token.id == "lexbuf") continue;

        if (spec.tokensDeclarados.count(token.id) == 0) {
            std::cerr << "Error: el token producido por YALex no fue declarado en YAPar: "
                      << token.id
                      << " con valor '" << token.valor << "'";

            if (i < posiciones.size()) {
                std::cerr << " en linea " << posiciones[i].linea
                          << ", columna " << posiciones[i].columna;
            }

            std::cerr << "\n";
        }
    }
}

void validarTokensIgnorados(const YaparSpec& spec) {
    for (const std::string& ignorado : spec.tokensIgnorados) {
        if (ignorado == "$") {
            throw std::runtime_error(
                "Error en YAPar: no se puede ignorar el token final $"
            );
        }
        if (spec.tokensDeclarados.count(ignorado) == 0) {
            throw std::runtime_error(
                "Error en YAPar: el token " + ignorado
                + " aparece en IGNORE pero no fue declarado con %token"
            );
        }
    }
}

void advertirTokensDeclaradosNoUsados(
    const std::vector<Token>& tokens,
    const YaparSpec& spec)
{
    std::set<std::string> producidos;
    for (const Token& token : tokens) {
        if (token.id != "$" && token.id != "lexbuf") producidos.insert(token.id);
    }

    for (const std::string& declarado : spec.tokensDeclarados) {
        if (spec.tokensIgnorados.count(declarado) > 0) continue;
        if (producidos.count(declarado) == 0) {
            std::cerr << "Advertencia: el token " << declarado
                      << " fue declarado en YAPar pero no aparece en esta entrada\n";
        }
    }
}

std::set<std::string> terminalesParaParsing(const YaparSpec& spec) {
    std::set<std::string> resultado;
    for (const std::string& token : spec.tokensDeclarados) {
        if (spec.tokensIgnorados.count(token) == 0) {
            resultado.insert(token);
        }
    }
    resultado.insert("$");
    return resultado;
}