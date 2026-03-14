/*
 * YALex Parser — Fases 1 y 2 del Generador de Analizadores Léxicos
 *
 * Fase 1: Parsing del archivo .yal
 * Fase 2: Expansión recursiva de macros (let), construcción de la
 *         mega-regex unificada con marcadores de token, visualización
 *         del Árbol de Expresión combinado.
 *
 * Compilar:
 *   g++ -std=c++17 -o yalex_parser YalexParser.cpp -O2
 *
 * Uso:
 *   ./yalex_parser archivo.yal [-o nombre_salida]
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <queue>
#include <algorithm>
#include <sstream>
#include <memory>
#include <functional>
#include <cassert>
#include <iomanip>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(dir) _mkdir(dir)
#else
#include <sys/stat.h>
#define MKDIR(dir) mkdir(dir, 0755)
#endif


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 1 — Estructuras de datos                          ║
// ╚══════════════════════════════════════════════════════════════╝

struct ReglaLexica {
    std::string regex_original;
    std::string regex_expandida;
    std::string accion;
    int prioridad;
    std::string nombre_token;
};

struct ArchivoYalex {
    std::string header;
    std::vector<std::pair<std::string, std::string>> definiciones_raw;
    std::map<std::string, std::string> definiciones_expandidas;
    std::string nombre_regla;
    std::vector<std::string> argumentos_regla;
    std::vector<ReglaLexica> reglas;
    std::string trailer;
    std::string mega_regex;
};


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 2 — Utilidades                                    ║
// ╚══════════════════════════════════════════════════════════════╝

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

static std::string leer_archivo(const std::string& ruta) {
    std::ifstream f(ruta);
    if (!f.is_open()) { std::cerr << "Error: No se pudo abrir '" << ruta << "'\n"; exit(1); }
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

static std::string dot_escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"' || c == '\\') r += '\\';
        if (c == '\n') { r += "\\n"; continue; }
        if (c == '\t') { r += "\\t"; continue; }
        if (c == '\r') { r += "\\r"; continue; }
        r += c;
    }
    return r;
}

static std::string nombre_simbolo(const std::string& s) {
    if (s == "\n") return "\\n";
    if (s == "\t") return "\\t";
    if (s == "\r") return "\\r";
    if (s == " ")  return "' '";
    return s;
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 3 — Eliminación de comentarios                    ║
// ╚══════════════════════════════════════════════════════════════╝

static std::string eliminar_comentarios(const std::string& src) {
    std::string r; r.reserve(src.size());
    size_t i = 0; int nivel = 0;
    while (i < src.size()) {
        if (nivel == 0 && (src[i] == '\'' || src[i] == '"')) {
            char d = src[i]; r += src[i++];
            while (i < src.size() && src[i] != d) { if (src[i] == '\\' && i+1 < src.size()) r += src[i++]; r += src[i++]; }
            if (i < src.size()) r += src[i++];
            continue;
        }
        if (i+1 < src.size() && src[i] == '(' && src[i+1] == '*') { nivel++; i += 2; continue; }
        if (i+1 < src.size() && src[i] == '*' && src[i+1] == ')') { if (nivel > 0) nivel--; i += 2; continue; }
        if (nivel == 0) r += src[i];
        i++;
    }
    return r;
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 4 — Extracción de bloques { }                     ║
// ╚══════════════════════════════════════════════════════════════╝

static size_t extraer_bloque_llaves(const std::string& src, size_t pos, std::string& contenido) {
    while (pos < src.size() && src[pos] != '{') { if (!isspace(src[pos])) return std::string::npos; pos++; }
    if (pos >= src.size()) return std::string::npos;
    int nivel = 0; size_t inicio = pos + 1; size_t i = pos;
    while (i < src.size()) {
        if (src[i] == '{') nivel++;
        else if (src[i] == '}') { nivel--; if (nivel == 0) { contenido = src.substr(inicio, i - inicio); return i + 1; } }
        else if (src[i] == '\'' || src[i] == '"') { char d = src[i]; i++; while (i < src.size() && src[i] != d) { if (src[i] == '\\') i++; i++; } }
        i++;
    }
    return std::string::npos;
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 5 — Parsing de caracteres, strings, conjuntos     ║
// ╚══════════════════════════════════════════════════════════════╝

static std::string interpretar_caracter_yalex(const std::string& token) {
    if (token.size() >= 2 && token.front() == '\'' && token.back() == '\'') {
        std::string in = token.substr(1, token.size() - 2);
        if (in.empty()) return "";
        if (in[0] == '\\' && in.size() >= 2) {
            switch (in[1]) {
                case 'n': return "\n"; case 't': return "\t"; case 'r': return "\r";
                case 's': return " "; case '\\': return "\\"; case '\'': return "'"; case '"': return "\"";
                default: return in.substr(1);
            }
        }
        return in;
    }
    return token;
}

static std::string expandir_cadena_yalex(const std::string& token) {
    if (token.size() < 2 || token.front() != '"' || token.back() != '"') return token;
    std::string in = token.substr(1, token.size() - 2);
    if (in.empty()) return "\\e"; // epsilon
    std::string r = "("; bool first = true; size_t i = 0;
    while (i < in.size()) {
        if (!first) r += "."; first = false;
        if (in[i] == '\\' && i+1 < in.size()) { r += "\\"; r += in[i+1]; i += 2; }
        else {
            char c = in[i];
            if (c=='('||c==')'||c=='|'||c=='*'||c=='+'||c=='?'||c=='.'||c=='['||c==']') r += "\\";
            r += c; i++;
        }
    }
    r += ")"; return r;
}

static std::string expandir_conjunto_yalex(const std::string& contenido_set, bool complemento) {
    std::vector<char> chars;
    size_t i = 0; std::string src = trim(contenido_set);
    while (i < src.size()) {
        while (i < src.size() && isspace(src[i])) i++;
        if (i >= src.size()) break;
        if (src[i] == '\'') {
            i++; char c1;
            if (i < src.size() && src[i] == '\\') { i++; if (i < src.size()) { switch(src[i]){case 'n':c1='\n';break;case 't':c1='\t';break;case 'r':c1='\r';break;case 's':c1=' ';break;case '\\':c1='\\';break;case '\'':c1='\'';break;default:c1=src[i];} i++; } else c1='\\'; }
            else if (i < src.size()) { c1 = src[i++]; } else break;
            if (i < src.size() && src[i] == '\'') i++;
            size_t j = i; while (j < src.size() && isspace(src[j])) j++;
            if (j < src.size() && src[j] == '-') {
                j++; while (j < src.size() && isspace(src[j])) j++;
                if (j < src.size() && src[j] == '\'') {
                    j++; char c2;
                    if (j < src.size() && src[j] == '\\') { j++; if (j < src.size()) { switch(src[j]){case 'n':c2='\n';break;case 't':c2='\t';break;case 'r':c2='\r';break;case 's':c2=' ';break;case '\\':c2='\\';break;case '\'':c2='\'';break;default:c2=src[j];} j++; } else c2='\\'; }
                    else if (j < src.size()) { c2 = src[j++]; } else break;
                    if (j < src.size() && src[j] == '\'') j++;
                    for (char c = c1; c <= c2; c++) chars.push_back(c);
                    i = j; continue;
                }
            }
            chars.push_back(c1); continue;
        }
        if (src[i] == '_') { for (char c = 32; c <= 126; c++) chars.push_back(c); i++; continue; }
        i++;
    }
    if (chars.empty()) return "\\e";
    std::sort(chars.begin(), chars.end());
    chars.erase(std::unique(chars.begin(), chars.end()), chars.end());
    if (complemento) {
        std::set<char> exc(chars.begin(), chars.end()); chars.clear();
        for (char c = 32; c <= 126; c++) if (!exc.count(c)) chars.push_back(c);
    }
    std::string r = "("; bool first = true;
    for (char c : chars) {
        if (!first) r += "|"; first = false;
        if (c=='('||c==')'||c=='|'||c=='*'||c=='+'||c=='?'||c=='.'||c=='['||c==']'||c=='\\'||c=='#'||c=='^') r += "\\";
        r += c;
    }
    r += ")"; return r;
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 6 — Expansión de regex YALex (FASE 2 CORE)       ║
// ║  Expansión recursiva con detección de ciclos               ║
// ╚══════════════════════════════════════════════════════════════╝

static std::string expandir_regex_yalex(
    const std::string& regex_yalex,
    const std::map<std::string, std::string>& defs);

static std::string expandir_definicion(
    const std::string& nombre,
    const std::map<std::string, std::string>& raw_map,
    std::map<std::string, std::string>& expandidas,
    std::set<std::string>& en_expansion)
{
    auto it = expandidas.find(nombre);
    if (it != expandidas.end()) return it->second;
    if (en_expansion.count(nombre)) { std::cerr << "Error: Ciclo en let para '" << nombre << "'\n"; return "\\e"; }
    auto raw_it = raw_map.find(nombre);
    if (raw_it == raw_map.end()) { std::cerr << "Advertencia: '" << nombre << "' no definido.\n"; return nombre; }
    en_expansion.insert(nombre);

    // Pre-expandir dependencias
    std::string src = trim(raw_it->second);
    size_t i = 0;
    while (i < src.size()) {
        if (src[i]=='\''||src[i]=='"') { char d=src[i]; i++; while(i<src.size()&&src[i]!=d){if(src[i]=='\\')i++;i++;} if(i<src.size())i++; continue; }
        if (src[i]=='[') { int n=1;i++; while(i<src.size()&&n>0){if(src[i]=='[')n++;else if(src[i]==']')n--;else if(src[i]=='\''){i++;while(i<src.size()&&src[i]!='\''){if(src[i]=='\\')i++;i++;}} i++;} continue; }
        if (isalpha(src[i])||src[i]=='_') {
            size_t s=i; while(i<src.size()&&(isalnum(src[i])||src[i]=='_'))i++;
            std::string id=src.substr(s,i-s);
            if (id!="eof"&&raw_map.count(id)&&!expandidas.count(id))
                expandir_definicion(id, raw_map, expandidas, en_expansion);
            continue;
        }
        i++;
    }

    std::string exp = expandir_regex_yalex(raw_it->second, expandidas);
    expandidas[nombre] = exp;
    en_expansion.erase(nombre);
    return exp;
}

static void expandir_todas_definiciones(
    const std::vector<std::pair<std::string, std::string>>& raw_vec,
    std::map<std::string, std::string>& expandidas)
{
    std::map<std::string, std::string> raw_map;
    for (auto& [n, r] : raw_vec) raw_map[n] = r;
    std::set<std::string> en_exp;
    for (auto& [n, r] : raw_vec)
        if (!expandidas.count(n)) expandir_definicion(n, raw_map, expandidas, en_exp);
}

static std::string expandir_regex_yalex(
    const std::string& regex_yalex,
    const std::map<std::string, std::string>& defs)
{
    std::vector<std::string> tokens;
    size_t i = 0; std::string src = trim(regex_yalex);

    while (i < src.size()) {
        if (isspace(src[i])) { i++; continue; }
        if (src[i] == '\'') {
            size_t s=i; i++; if(i<src.size()&&src[i]=='\\')i+=2; else if(i<src.size())i++;
            if(i<src.size()&&src[i]=='\'')i++;
            std::string raw=src.substr(s,i-s);
            std::string ch=interpretar_caracter_yalex(raw);
            std::string esc;
            for(char c:ch){if(c=='('||c==')'||c=='|'||c=='*'||c=='+'||c=='?'||c=='.'||c=='['||c==']'||c=='\\'||c=='#'||c=='^')esc+="\\";esc+=c;}
            tokens.push_back(esc); continue;
        }
        if (src[i] == '"') {
            size_t s=i; i++; while(i<src.size()&&src[i]!='"'){if(src[i]=='\\')i++;i++;} if(i<src.size())i++;
            tokens.push_back(expandir_cadena_yalex(src.substr(s,i-s))); continue;
        }
        if (src[i] == '[') {
            i++; bool comp=false;
            while(i<src.size()&&isspace(src[i]))i++;
            if(i<src.size()&&src[i]=='^'){comp=true;i++;}
            if(i+1<src.size()&&(unsigned char)src[i]==0xCB&&(unsigned char)src[i+1]==0x86){comp=true;i+=2;}
            int n=1;size_t sc=i;
            while(i<src.size()&&n>0){if(src[i]=='[')n++;else if(src[i]==']'){n--;if(n==0)break;}else if(src[i]=='\''){i++;while(i<src.size()&&src[i]!='\''){if(src[i]=='\\')i++;i++;}} i++;}
            std::string cont=src.substr(sc,i-sc);
            if(i<src.size()&&src[i]==']')i++;
            tokens.push_back(expandir_conjunto_yalex(cont,comp)); continue;
        }
        if(src[i]=='|'){tokens.push_back("|");i++;continue;}
        if(src[i]=='*'){tokens.push_back("*");i++;continue;}
        if(src[i]=='+'){tokens.push_back("+");i++;continue;}
        if(src[i]=='?'){tokens.push_back("?");i++;continue;}
        if(src[i]=='('){tokens.push_back("(");i++;continue;}
        if(src[i]==')'){tokens.push_back(")");i++;continue;}
        if(src[i]=='#'){tokens.push_back("#");i++;continue;}
        if(isalpha(src[i])||src[i]=='_') {
            size_t s=i; while(i<src.size()&&(isalnum(src[i])||src[i]=='_'))i++;
            std::string id=src.substr(s,i-s);
            if(id=="eof"){tokens.push_back("\\eof");continue;}
            auto it=defs.find(id);
            if(it!=defs.end()) tokens.push_back("("+it->second+")");
            else { std::cerr<<"Advertencia: '"<<id<<"' no definido.\n"; for(char c:id)tokens.push_back(std::string(1,c)); }
            continue;
        }
        char c=src[i]; std::string s;
        if(c=='('||c==')'||c=='|'||c=='*'||c=='+'||c=='?'||c=='.'||c=='['||c==']'||c=='\\'||c=='#'||c=='^') s="\\";
        s+=c; tokens.push_back(s); i++;
    }

    static const std::set<std::string> nda={"(", "|", "#", "."};
    static const std::set<std::string> ndb={")", "|", "*", "+", "?", "#", "."};
    std::string r;
    for(size_t j=0;j<tokens.size();j++){
        r+=tokens[j];
        if(j+1<tokens.size()){
            bool ins=true;
            if(nda.count(tokens[j]))ins=false;
            if(ndb.count(tokens[j+1]))ins=false;
            if(ins)r+=".";
        }
    }
    return r;
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 7 — Parser del archivo YALex                      ║
// ╚══════════════════════════════════════════════════════════════╝

ArchivoYalex parsear_yalex(const std::string& ruta) {
    ArchivoYalex res;
    std::string contenido = eliminar_comentarios(leer_archivo(ruta));
    std::cout << "=== FASE 1: PARSEANDO ARCHIVO YALEX ===\n";
    std::cout << "Archivo: " << ruta << "\n\n";
    size_t pos = 0;

    { size_t t=pos; while(t<contenido.size()&&isspace(contenido[t]))t++;
      if(t<contenido.size()&&contenido[t]=='{'){std::string h; size_t f=extraer_bloque_llaves(contenido,t,h); if(f!=std::string::npos){res.header=trim(h);pos=f;std::cout<<"[Header] encontrado\n";}}}

    while (pos < contenido.size()) {
        while(pos<contenido.size()&&isspace(contenido[pos]))pos++;
        if(pos>=contenido.size())break;
        if(pos+3<=contenido.size()&&contenido.substr(pos,3)=="let"&&(pos+3>=contenido.size()||isspace(contenido[pos+3]))) {
            pos+=3; while(pos<contenido.size()&&isspace(contenido[pos]))pos++;
            size_t si=pos; while(pos<contenido.size()&&(isalnum(contenido[pos])||contenido[pos]=='_'))pos++;
            std::string nombre=contenido.substr(si,pos-si);
            while(pos<contenido.size()&&isspace(contenido[pos]))pos++;
            if(pos<contenido.size()&&contenido[pos]=='=')pos++;
            while(pos<contenido.size()&&isspace(contenido[pos]))pos++;
            size_t ri=pos,rf=pos;
            while(pos<contenido.size()){
                if(contenido[pos]=='\n'){size_t t=pos+1;while(t<contenido.size()&&(contenido[t]==' '||contenido[t]=='\t'))t++;
                    if(t+3<=contenido.size()&&contenido.substr(t,3)=="let"&&(t+3>=contenido.size()||isspace(contenido[t+3]))){rf=pos;pos=t;break;}
                    if(t+4<=contenido.size()&&contenido.substr(t,4)=="rule"&&(t+4>=contenido.size()||isspace(contenido[t+4]))){rf=pos;pos=t;break;}
                }
                rf=pos+1;pos++;
            }
            res.definiciones_raw.push_back({nombre,trim(contenido.substr(ri,rf-ri))});
            std::cout<<"[let] "<<nombre<<" = "<<trim(contenido.substr(ri,rf-ri))<<"\n";
            continue;
        }
        if(pos+4<=contenido.size()&&contenido.substr(pos,4)=="rule"&&(pos+4>=contenido.size()||isspace(contenido[pos+4]))) {
            pos+=4; while(pos<contenido.size()&&isspace(contenido[pos]))pos++;
            size_t ni=pos; while(pos<contenido.size()&&(isalnum(contenido[pos])||contenido[pos]=='_'))pos++;
            res.nombre_regla=contenido.substr(ni,pos-ni);
            while(pos<contenido.size()&&contenido[pos]!='=')pos++;
            if(pos<contenido.size())pos++;
            std::cout<<"\n[rule] "<<res.nombre_regla<<" =\n";
            int prio=0;
            while(pos<contenido.size()){
                while(pos<contenido.size()&&(isspace(contenido[pos])||contenido[pos]=='|'))pos++;
                if(pos>=contenido.size())break;
                size_t ri=pos; int bl=0;
                while(pos<contenido.size()){
                    if(contenido[pos]=='[')bl++;else if(contenido[pos]==']')bl--;
                    else if(contenido[pos]=='{'&&bl==0)break;
                    else if(contenido[pos]=='\''&&bl==0){pos++;while(pos<contenido.size()&&contenido[pos]!='\''){if(contenido[pos]=='\\')pos++;pos++;}}
                    else if(contenido[pos]=='"'&&bl==0){pos++;while(pos<contenido.size()&&contenido[pos]!='"'){if(contenido[pos]=='\\')pos++;pos++;}}
                    pos++;
                }
                std::string rx=trim(contenido.substr(ri,pos-ri));
                if(rx.empty()){
                    if(pos<contenido.size()&&contenido[pos]=='{'){std::string tc;size_t f=extraer_bloque_llaves(contenido,pos,tc);if(f!=std::string::npos){res.trailer=trim(tc);pos=f;std::cout<<"\n[Trailer] encontrado\n";}}
                    break;
                }
                std::string accion;
                if(pos<contenido.size()&&contenido[pos]=='{'){size_t f=extraer_bloque_llaves(contenido,pos,accion);if(f!=std::string::npos)pos=f;}
                ReglaLexica regla; regla.regex_original=rx; regla.accion=trim(accion); regla.prioridad=prio++;
                regla.nombre_token="TOKEN_"+std::to_string(regla.prioridad);
                if(regla.accion.find("return ")!=std::string::npos){size_t p=regla.accion.find("return ")+7;size_t f=regla.accion.find_first_of(" ;)\n",p);if(f==std::string::npos)f=regla.accion.size();regla.nombre_token=regla.accion.substr(p,f-p);}
                res.reglas.push_back(regla);
                std::cout<<"  "<<prio<<". /"<<rx<<"/  ->  { "<<regla.accion<<" }  ["<<regla.nombre_token<<"]\n";
            }
            continue;
        }
        if(contenido[pos]=='{'){std::string tc;size_t f=extraer_bloque_llaves(contenido,pos,tc);if(f!=std::string::npos){res.trailer=trim(tc);pos=f;std::cout<<"\n[Trailer] encontrado\n";}else pos++;continue;}
        pos++;
    }
    return res;
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 8 — FASE 2: Expansión + mega-regex                ║
// ╚══════════════════════════════════════════════════════════════╝

static std::string construir_mega_regex(const std::vector<ReglaLexica>& reglas) {
    // (regex_0.#T0#)|(regex_1.#T1#)|...
    std::string mega;
    for (size_t i = 0; i < reglas.size(); i++) {
        if (i > 0) mega += "|";
        mega += "(" + reglas[i].regex_expandida + ".\\#T" + std::to_string(i) + "\\#)";
    }
    return mega;
}

void fase2_expandir_y_unificar(ArchivoYalex& yalex) {
    std::cout << "\n╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║  FASE 2: EXPANSIÓN DE MACROS Y MEGA-REGEX           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n\n";

    // 1) Expandir todas las definiciones let recursivamente
    std::cout << "--- Expansión recursiva de definiciones let ---\n\n";
    expandir_todas_definiciones(yalex.definiciones_raw, yalex.definiciones_expandidas);
    for (auto& [nombre, rx] : yalex.definiciones_raw) {
        auto it = yalex.definiciones_expandidas.find(nombre);
        if (it != yalex.definiciones_expandidas.end()) {
            std::cout << "  let " << nombre << "\n";
            std::cout << "    Raw:       " << rx << "\n";
            std::cout << "    Expandida: " << it->second << "\n\n";
        }
    }

    // 2) Expandir cada regla léxica
    std::cout << "--- Expansión de patrones de reglas léxicas ---\n\n";
    for (size_t i = 0; i < yalex.reglas.size(); i++) {
        yalex.reglas[i].regex_expandida = expandir_regex_yalex(yalex.reglas[i].regex_original, yalex.definiciones_expandidas);
        std::cout << "  Regla " << i << " [" << yalex.reglas[i].nombre_token << "]\n";
        std::cout << "    Original:  " << yalex.reglas[i].regex_original << "\n";
        std::cout << "    Expandida: " << yalex.reglas[i].regex_expandida << "\n\n";
    }

    // 3) Mega-regex unificada
    std::cout << "--- Mega-regex unificada ---\n\n";
    yalex.mega_regex = construir_mega_regex(yalex.reglas);
    if (yalex.mega_regex.size() <= 500) std::cout << "  " << yalex.mega_regex << "\n\n";
    else std::cout << "  (" << yalex.mega_regex.size() << " chars, truncado)\n  " << yalex.mega_regex.substr(0,200) << " ...\n\n";

    // Tabla de tokens
    std::cout << "  Tabla de tokens:\n";
    std::cout << "  " << std::string(55, '-') << "\n";
    std::cout << "  " << std::left << std::setw(5) << "ID" << std::setw(18) << "Token" << "Acción\n";
    std::cout << "  " << std::string(55, '-') << "\n";
    for (size_t i = 0; i < yalex.reglas.size(); i++)
        std::cout << "  " << std::setw(5) << i << std::setw(18) << yalex.reglas[i].nombre_token << yalex.reglas[i].accion << "\n";
    std::cout << "  " << std::string(55, '-') << "\n";
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 9 — Nodo AST, AFN, AFD                            ║
// ╚══════════════════════════════════════════════════════════════╝

struct NodoAST {
    std::string valor;
    std::shared_ptr<NodoAST> izq, der;
    NodoAST(const std::string& v, std::shared_ptr<NodoAST> i=nullptr, std::shared_ptr<NodoAST> d=nullptr) : valor(v), izq(i), der(d) {}
};

struct EstadoAFN {
    int id; bool es_final; int token_id;
    std::map<std::string, std::vector<EstadoAFN*>> transiciones;
    EstadoAFN(int i) : id(i), es_final(false), token_id(-1) {}
    void agregar_transicion(const std::string& s, EstadoAFN* d) { transiciones[s].push_back(d); }
};

struct AFN {
    EstadoAFN* estado_inicial = nullptr;
    std::vector<EstadoAFN*> estados_finales;
    std::vector<std::unique_ptr<EstadoAFN>> estados;
    std::set<std::string> alfabeto;
    int contador_estados = 0;
    AFN()=default; AFN(const AFN&)=delete; AFN& operator=(const AFN&)=delete; AFN(AFN&&)=default; AFN& operator=(AFN&&)=default;
    EstadoAFN* nuevo_estado() { auto e=std::make_unique<EstadoAFN>(contador_estados++); auto* p=e.get(); estados.push_back(std::move(e)); return p; }
    void agregar_simbolo(const std::string& s) { if(s!="ε") alfabeto.insert(s); }
};

struct FragmentoAFN { EstadoAFN* inicio; EstadoAFN* fin; };

struct EstadoAFD {
    int id; std::set<int> conjunto_afn; std::vector<EstadoAFN*> estados_afn;
    bool es_final; int token_id;
    std::map<std::string, EstadoAFD*> transiciones;
    EstadoAFD(int i, const std::vector<EstadoAFN*>& c) : id(i), estados_afn(c), es_final(false), token_id(-1) {
        for(auto* e:c){conjunto_afn.insert(e->id);if(e->es_final){es_final=true;if(e->token_id>=0&&(token_id<0||e->token_id<token_id))token_id=e->token_id;}}
    }
    void agregar_transicion(const std::string& s, EstadoAFD* d) { transiciones[s]=d; }
    std::string repr() const { return "q"+std::to_string(id); }
};

struct AFD {
    EstadoAFD* estado_inicial = nullptr;
    std::vector<EstadoAFD*> estados_finales;
    std::vector<std::unique_ptr<EstadoAFD>> estados;
    std::set<std::string> alfabeto;
    int contador_estados = 0;
    AFD()=default; AFD(const AFD&)=delete; AFD& operator=(const AFD&)=delete; AFD(AFD&&)=default; AFD& operator=(AFD&&)=default;
    EstadoAFD* nuevo_estado(const std::vector<EstadoAFN*>& c) {
        auto e=std::make_unique<EstadoAFD>(contador_estados++,c); auto* p=e.get(); estados.push_back(std::move(e));
        if(p->es_final) estados_finales.push_back(p); return p;
    }
};


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 10 — Thompson, Subconjuntos, Minimización         ║
// ╚══════════════════════════════════════════════════════════════╝

std::set<EstadoAFN*> clausura_epsilon(const std::vector<EstadoAFN*>& ini) {
    std::set<EstadoAFN*> cl(ini.begin(), ini.end());
    std::stack<EstadoAFN*> st; for(auto* e:ini)st.push(e);
    while(!st.empty()){auto* a=st.top();st.pop();auto it=a->transiciones.find("ε");if(it!=a->transiciones.end())for(auto* d:it->second)if(!cl.count(d)){cl.insert(d);st.push(d);}}
    return cl;
}

static const std::set<std::string> OPERADORES = {"|",".",  "*","+","?","^","#"};

AFN construir_thompson_afn(std::shared_ptr<NodoAST> ast) {
    AFN afn; if(!ast) return afn;
    std::function<FragmentoAFN(std::shared_ptr<NodoAST>)> B=[&](std::shared_ptr<NodoAST> n)->FragmentoAFN{
        if(!OPERADORES.count(n->valor)){auto* s=afn.nuevo_estado();auto* f=afn.nuevo_estado();std::string sy=n->valor;if(!sy.empty()&&sy[0]=='\\')sy=sy.substr(1);if(sy=="ε")s->agregar_transicion("ε",f);else{afn.agregar_simbolo(sy);s->agregar_transicion(sy,f);}return{s,f};}
        if(n->valor=="."){auto l=B(n->izq);auto r=B(n->der);l.fin->agregar_transicion("ε",r.inicio);return{l.inicio,r.fin};}
        if(n->valor=="|"){auto l=B(n->izq);auto r=B(n->der);auto* s=afn.nuevo_estado();auto* f=afn.nuevo_estado();s->agregar_transicion("ε",l.inicio);s->agregar_transicion("ε",r.inicio);l.fin->agregar_transicion("ε",f);r.fin->agregar_transicion("ε",f);return{s,f};}
        if(n->valor=="*"){auto c=B(n->izq);auto* s=afn.nuevo_estado();auto* f=afn.nuevo_estado();s->agregar_transicion("ε",c.inicio);s->agregar_transicion("ε",f);c.fin->agregar_transicion("ε",c.inicio);c.fin->agregar_transicion("ε",f);return{s,f};}
        if(n->valor=="+"){auto c=B(n->izq);auto* s=afn.nuevo_estado();auto* f=afn.nuevo_estado();s->agregar_transicion("ε",c.inicio);c.fin->agregar_transicion("ε",c.inicio);c.fin->agregar_transicion("ε",f);return{s,f};}
        if(n->valor=="?"){auto c=B(n->izq);auto* s=afn.nuevo_estado();auto* f=afn.nuevo_estado();s->agregar_transicion("ε",c.inicio);s->agregar_transicion("ε",f);c.fin->agregar_transicion("ε",f);return{s,f};}
        auto* s=afn.nuevo_estado();auto* f=afn.nuevo_estado();s->agregar_transicion(n->valor,f);afn.agregar_simbolo(n->valor);return{s,f};
    };
    auto frag=B(ast); afn.estado_inicial=frag.inicio; frag.fin->es_final=true; afn.estados_finales.push_back(frag.fin);
    return afn;
}

AFD construir_afd_subconjuntos(AFN& afn) {
    AFD afd; afd.alfabeto=afn.alfabeto;
    auto cl=clausura_epsilon({afn.estado_inicial}); std::vector<EstadoAFN*> vi(cl.begin(),cl.end());
    auto* q0=afd.nuevo_estado(vi); afd.estado_inicial=q0;
    std::map<std::set<int>,EstadoAFD*> vis; vis[q0->conjunto_afn]=q0;
    std::queue<EstadoAFD*> cola; cola.push(q0);
    while(!cola.empty()){auto* cur=cola.front();cola.pop();
        for(auto& sym:afd.alfabeto){std::set<EstadoAFN*> dest;
            for(auto* e:cur->estados_afn){auto it=e->transiciones.find(sym);if(it!=e->transiciones.end())for(auto* d:it->second)dest.insert(d);}
            if(dest.empty())continue;
            std::vector<EstadoAFN*> dv(dest.begin(),dest.end());auto c2=clausura_epsilon(dv);std::vector<EstadoAFN*> cv(c2.begin(),c2.end());
            std::set<int> ids;for(auto* e:cv)ids.insert(e->id);
            auto it=vis.find(ids);EstadoAFD* tgt;
            if(it!=vis.end())tgt=it->second;else{tgt=afd.nuevo_estado(cv);vis[ids]=tgt;cola.push(tgt);}
            cur->agregar_transicion(sym,tgt);}}
    return afd;
}

AFD eliminar_estados_inalcanzables(AFD& afd) {
    std::set<EstadoAFD*> alc; std::queue<EstadoAFD*> q; alc.insert(afd.estado_inicial); q.push(afd.estado_inicial);
    while(!q.empty()){auto* c=q.front();q.pop();for(auto&[s,d]:c->transiciones)if(!alc.count(d)){alc.insert(d);q.push(d);}}
    AFD r;r.alfabeto=afd.alfabeto;std::map<EstadoAFD*,EstadoAFD*>m;
    for(auto* e:alc){auto* n=r.nuevo_estado(e->estados_afn);n->es_final=e->es_final;n->token_id=e->token_id;m[e]=n;}
    r.estados_finales.clear();for(auto&[o,n]:m)if(n->es_final)r.estados_finales.push_back(n);
    r.estado_inicial=m[afd.estado_inicial];
    for(auto&[o,n]:m)for(auto&[s,d]:o->transiciones){auto it=m.find(d);if(it!=m.end())n->transiciones[s]=it->second;}
    r.contador_estados=(int)r.estados.size();return r;
}

AFD eliminar_estados_muertos(AFD& afd) {
    std::set<EstadoAFD*> vivos(afd.estados_finales.begin(),afd.estados_finales.end());
    bool cambio=true;while(cambio){cambio=false;for(auto&up:afd.estados){auto*e=up.get();if(vivos.count(e))continue;for(auto&[s,d]:e->transiciones)if(vivos.count(d)){vivos.insert(e);cambio=true;break;}}}
    if(!vivos.count(afd.estado_inicial)){AFD v;v.alfabeto=afd.alfabeto;v.estado_inicial=v.nuevo_estado({});return v;}
    AFD r;r.alfabeto=afd.alfabeto;std::map<EstadoAFD*,EstadoAFD*>m;
    for(auto*e:vivos){auto*n=r.nuevo_estado(e->estados_afn);n->es_final=e->es_final;n->token_id=e->token_id;m[e]=n;}
    r.estados_finales.clear();for(auto&[o,n]:m)if(n->es_final)r.estados_finales.push_back(n);
    r.estado_inicial=m[afd.estado_inicial];
    for(auto&[o,n]:m)for(auto&[s,d]:o->transiciones){auto it=m.find(d);if(it!=m.end())n->transiciones[s]=it->second;}
    r.contador_estados=(int)r.estados.size();return r;
}

static int particion_de(EstadoAFD* e, const std::vector<std::set<EstadoAFD*>>& P) { for(int i=0;i<(int)P.size();i++)if(P[i].count(e))return i;return -1; }

std::vector<std::set<EstadoAFD*>> dividir_particion(const std::set<EstadoAFD*>& part, const std::vector<std::set<EstadoAFD*>>& todas, const std::set<std::string>& alfa) {
    if(part.size()<=1)return{part};
    std::map<std::vector<std::pair<std::string,int>>,std::set<EstadoAFD*>> grupos;
    for(auto*e:part){std::vector<std::pair<std::string,int>>firma;for(auto&s:alfa){auto it=e->transiciones.find(s);firma.push_back({s,it!=e->transiciones.end()?particion_de(it->second,todas):-1});}firma.push_back({"__tk__",e->token_id});grupos[firma].insert(e);}
    std::vector<std::set<EstadoAFD*>>r;for(auto&[k,v]:grupos)r.push_back(v);return r;
}

AFD construir_afd_minimizado(AFD& orig, const std::vector<std::set<EstadoAFD*>>& P) {
    AFD r;r.alfabeto=orig.alfabeto;std::vector<EstadoAFD*>em(P.size(),nullptr);
    for(int i=0;i<(int)P.size();i++){auto*repr=*P[i].begin();std::set<int>comb;for(auto*e:P[i])for(int id:e->conjunto_afn)comb.insert(id);
        auto n=std::make_unique<EstadoAFD>(i,std::vector<EstadoAFN*>{});n->conjunto_afn=comb;n->es_final=repr->es_final;n->token_id=repr->token_id;
        auto*p=n.get();r.estados.push_back(std::move(n));em[i]=p;if(p->es_final)r.estados_finales.push_back(p);if(P[i].count(orig.estado_inicial))r.estado_inicial=p;}
    for(int i=0;i<(int)P.size();i++){auto*repr=*P[i].begin();for(auto&s:r.alfabeto){auto it=repr->transiciones.find(s);if(it==repr->transiciones.end())continue;int j=particion_de(it->second,P);if(j>=0)em[i]->agregar_transicion(s,em[j]);}}
    r.contador_estados=(int)r.estados.size();return r;
}

AFD minimizar_afd(AFD& afd) {
    auto a1=eliminar_estados_inalcanzables(afd);auto a2=eliminar_estados_muertos(a1);
    if(a2.estados.empty())return construir_afd_minimizado(a2,{});
    std::set<EstadoAFD*>F(a2.estados_finales.begin(),a2.estados_finales.end());
    std::set<EstadoAFD*>NF;for(auto&up:a2.estados)if(!F.count(up.get()))NF.insert(up.get());
    std::vector<std::set<EstadoAFD*>>P;if(!F.empty())P.push_back(F);if(!NF.empty())P.push_back(NF);
    bool cambio=true;while(cambio){cambio=false;std::vector<std::set<EstadoAFD*>>np;for(auto&p:P){auto subs=dividir_particion(p,P,a2.alfabeto);if(subs.size()>1)cambio=true;for(auto&s:subs)np.push_back(s);}P=np;}
    return construir_afd_minimizado(a2,P);
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 11 — Shunting Yard + AST                          ║
// ╚══════════════════════════════════════════════════════════════╝

int get_precedencia(const std::string& c) { if(c=="(")return 1;if(c=="|")return 2;if(c=="."||c=="#")return 3;if(c=="?"||c=="*"||c=="+")return 4;if(c=="^")return 5;return 0; }

std::vector<std::string> format_regex(const std::string& rx) {
    std::vector<std::string> res; size_t i=0;
    while(i<rx.size()){std::string c1;if(rx[i]=='\\'&&i+1<rx.size()){c1=rx.substr(i,2);i+=2;}else{unsigned char uc=(unsigned char)rx[i];int b=1;if((uc&0xE0)==0xC0)b=2;else if((uc&0xF0)==0xE0)b=3;else if((uc&0xF8)==0xF0)b=4;c1=rx.substr(i,b);i+=b;}
        res.push_back(c1);
        if(i<rx.size()){std::string c2;if(rx[i]=='\\'&&i+1<rx.size())c2=rx.substr(i,2);else{unsigned char u2=(unsigned char)rx[i];int b2=1;if((u2&0xE0)==0xC0)b2=2;else if((u2&0xF0)==0xE0)b2=3;else if((u2&0xF8)==0xF0)b2=4;c2=rx.substr(i,b2);}
            static const std::set<std::string>nda={"(", "|", "#", "."},ndb={")", "|", "?", "*", "+", "^", "#", "."},uc={"?", "*", "+"};
            if(!nda.count(c1)&&!ndb.count(c2)&&!uc.count(c1))res.push_back(".");}}
    return res;
}

std::shared_ptr<NodoAST> construir_AST(const std::vector<std::string>& tokens) {
    std::stack<std::shared_ptr<NodoAST>> pila;
    for(auto&t:tokens){if(t=="|"||t=="."||t=="^"||t=="#"){if(pila.size()<2)break;auto d=pila.top();pila.pop();auto iz=pila.top();pila.pop();pila.push(std::make_shared<NodoAST>(t,iz,d));}
        else if(t=="*"||t=="+"||t=="?"){if(pila.empty())break;auto h=pila.top();pila.pop();pila.push(std::make_shared<NodoAST>(t,h));}
        else pila.push(std::make_shared<NodoAST>(t));}
    if(pila.size()==1)return pila.top();
    while(pila.size()>1){auto d=pila.top();pila.pop();auto iz=pila.top();pila.pop();pila.push(std::make_shared<NodoAST>(".",iz,d));}
    return pila.empty()?nullptr:pila.top();
}

std::shared_ptr<NodoAST> regex_a_ast(const std::string& rx) {
    auto fmt=format_regex(rx);std::vector<std::string>out;std::stack<std::string>stk;
    static const std::set<std::string>OPS={"|",".","(",")","?","*","+","^","#"};
    for(auto&t:fmt){bool lit=t.size()>1||!OPS.count(t);bool esc=t.size()>=2&&t[0]=='\\';
        if(esc||lit){out.push_back(t);continue;}if(t=="*"||t=="+"||t=="?"){out.push_back(t);continue;}
        if(t=="("){stk.push(t);continue;}if(t==")"){while(!stk.empty()&&stk.top()!="("){out.push_back(stk.top());stk.pop();}if(!stk.empty())stk.pop();continue;}
        int prec=get_precedencia(t);while(!stk.empty()&&get_precedencia(stk.top())>=prec){out.push_back(stk.top());stk.pop();}stk.push(t);}
    while(!stk.empty()){out.push_back(stk.top());stk.pop();}
    return construir_AST(out);
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 12 — Graphviz                                     ║
// ╚══════════════════════════════════════════════════════════════╝

void dibujar_ast(std::shared_ptr<NodoAST> raiz, const std::string& nombre, const std::string& titulo="") {
    std::ofstream f(nombre+".dot");
    f<<"digraph AST {\n  rankdir=TB;\n  ranksep=0.8;\n";
    if(!titulo.empty())f<<"  labelloc=t;\n  label=\""<<dot_escape(titulo)<<"\";\n  fontsize=14;\n";
    int cnt=0;
    std::function<int(std::shared_ptr<NodoAST>)> add=[&](std::shared_ptr<NodoAST> n)->int{
        if(!n)return -1;int id=cnt++;
        f<<"  "<<id<<" [label=\""<<dot_escape(n->valor)<<"\"];\n";
        int l=add(n->izq);if(l>=0)f<<"  "<<id<<" -> "<<l<<";\n";
        int r=add(n->der);if(r>=0)f<<"  "<<id<<" -> "<<r<<";\n";
        return id;};
    add(raiz);f<<"}\n";
    std::cout<<"  AST: "<<nombre<<".dot\n";
}

void dibujar_afn(AFN& afn, const std::string& nombre) {
    std::ofstream f(nombre+".dot");
    f<<"digraph AFN {\n  rankdir=LR;\n  inicio [shape=none,label=\"\",width=0,height=0];\n";
    for(auto&up:afn.estados){auto*e=up.get();std::string sh=e->es_final?"doublecircle":"circle";
        std::string lb=std::to_string(e->id);if(e->es_final&&e->token_id>=0)lb+="\\n[T"+std::to_string(e->token_id)+"]";
        f<<"  "<<e->id<<" [shape="<<sh<<",label=\""<<lb<<"\"];\n";}
    if(afn.estado_inicial)f<<"  inicio -> "<<afn.estado_inicial->id<<";\n";
    for(auto&up:afn.estados){auto*e=up.get();for(auto&[s,ds]:e->transiciones)for(auto*d:ds)f<<"  "<<e->id<<" -> "<<d->id<<" [label=\""<<dot_escape(s)<<"\"];\n";}
    f<<"}\n";
}

void dibujar_afd(AFD& afd, const std::string& nombre) {
    std::ofstream f(nombre+".dot");
    f<<"digraph AFD {\n  rankdir=LR;\n  inicio_afd [shape=none,label=\"\",width=0,height=0];\n";
    for(auto&up:afd.estados){auto*e=up.get();std::string lb="q"+std::to_string(e->id);
        if(e->es_final&&e->token_id>=0)lb+="\\n[T"+std::to_string(e->token_id)+"]";
        f<<"  "<<e->id<<" [shape="<<(e->es_final?"doublecircle":"circle")<<",label=\""<<lb<<"\"];\n";}
    if(afd.estado_inicial)f<<"  inicio_afd -> "<<afd.estado_inicial->id<<";\n";
    for(auto&up:afd.estados){auto*e=up.get();for(auto&[s,d]:e->transiciones)f<<"  "<<e->id<<" -> "<<d->id<<" [label=\""<<dot_escape(s)<<"\"];\n";}
    f<<"}\n";
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 13 — AFN combinado                                ║
// ╚══════════════════════════════════════════════════════════════╝

AFN construir_afn_combinado(const std::vector<ReglaLexica>& reglas) {
    AFN comb; auto* ini=comb.nuevo_estado(); comb.estado_inicial=ini;
    for(size_t i=0;i<reglas.size();i++){
        auto ast=regex_a_ast(reglas[i].regex_expandida);
        if(!ast){std::cerr<<"Error: AST nulo regla "<<i<<"\n";continue;}
        AFN sub=construir_thompson_afn(ast);
        if(!sub.estado_inicial){std::cerr<<"Error: AFN vacío regla "<<i<<"\n";continue;}
        for(auto&s:sub.alfabeto)comb.agregar_simbolo(s);
        std::map<int,EstadoAFN*>m;
        for(auto&up:sub.estados){auto*n=comb.nuevo_estado();n->es_final=up->es_final;if(n->es_final){n->token_id=(int)i;comb.estados_finales.push_back(n);}m[up->id]=n;}
        for(auto&up:sub.estados){auto*o=m[up->id];for(auto&[s,ds]:up->transiciones)for(auto*d:ds)o->agregar_transicion(s,m[d->id]);}
        ini->agregar_transicion("ε",m[sub.estado_inicial->id]);
    }
    return comb;
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 14 — Generación de código                         ║
// ╚══════════════════════════════════════════════════════════════╝

void generar_analizador_lexico(const ArchivoYalex& yalex, AFD& afd_min, const std::string& nombre_salida) {
    std::ofstream o(nombre_salida+".cpp");

    // ── Encabezado ──
    o << "/*\n";
    o << " * Analizador Lexico generado automaticamente desde YALex\n";
    o << " * Compilar: g++ -std=c++17 -o " << nombre_salida << " " << nombre_salida << ".cpp\n";
    o << " * Uso: ./" << nombre_salida << " <archivo_entrada>\n";
    o << " *\n";
    o << " * Interfaz para parser:\n";
    o << " *   Token getNextToken()  - retorna el siguiente token\n";
    o << " *   Los tokens tienen: tipo, lexema, linea, columna\n";
    o << " */\n\n";

    o << "#include <iostream>\n";
    o << "#include <fstream>\n";
    o << "#include <string>\n";
    o << "#include <vector>\n";
    o << "#include <sstream>\n\n";

    // ── Header del usuario ──
    if (!yalex.header.empty()) {
        o << "// ========== HEADER DEL USUARIO ==========\n";
        o << yalex.header << "\n";
        o << "// ========== FIN HEADER ==========\n\n";
    }

    // ── Enum de tipos de token ──
    o << "// ========== TIPOS DE TOKEN ==========\n";
    o << "enum TokenType {\n";
    // Recolectar nombres únicos
    std::vector<std::string> nombres_token;
    std::set<std::string> nombres_vistos;
    for (size_t i = 0; i < yalex.reglas.size(); i++) {
        std::string name = yalex.reglas[i].nombre_token;
        // Sanitizar: solo letras, digitos, _
        std::string safe;
        for (char c : name) {
            if (isalnum(c) || c == '_') safe += c;
        }
        if (safe.empty()) safe = "TOKEN_" + std::to_string(i);
        // Evitar duplicados
        std::string original = safe;
        int dup = 1;
        while (nombres_vistos.count(safe)) {
            safe = original + "_" + std::to_string(dup++);
        }
        nombres_vistos.insert(safe);
        nombres_token.push_back(safe);
    }

    for (size_t i = 0; i < nombres_token.size(); i++) {
        o << "    TOK_" << nombres_token[i] << " = " << i;
        if (i + 1 < nombres_token.size()) o << ",";
        o << "\n";
    }
    o << "};\n\n";

    o << "const int NUM_TOKEN_TYPES = " << nombres_token.size() << ";\n\n";

    // ── Tabla de nombres (para imprimir) ──
    o << "const char* TOKEN_TYPE_NAMES[] = {\n";
    for (size_t i = 0; i < nombres_token.size(); i++) {
        o << "    \"" << nombres_token[i] << "\"";
        if (i + 1 < nombres_token.size()) o << ",";
        o << "\n";
    }
    o << "};\n\n";

    // ── Acciones originales (para lógica de skip) ──
    o << "const char* TOKEN_ACTIONS[] = {\n";
    for (size_t i = 0; i < yalex.reglas.size(); i++) {
        std::string esc;
        for (char c : yalex.reglas[i].accion) {
            if (c == '"') esc += "\\\"";
            else if (c == '\\') esc += "\\\\";
            else if (c == '\n') esc += "\\n";
            else esc += c;
        }
        o << "    \"" << esc << "\"";
        if (i + 1 < yalex.reglas.size()) o << ",";
        o << "\n";
    }
    o << "};\n\n";

    // ── Struct Token ──
    o << "// ========== STRUCT TOKEN ==========\n";
    o << "struct Token {\n";
    o << "    TokenType tipo;\n";
    o << "    std::string lexema;\n";
    o << "    int linea;\n";
    o << "    int columna;\n";
    o << "\n";
    o << "    // Token especial para fin de archivo\n";
    o << "    bool esFinArchivo() const { return tipo == (TokenType)-1; }\n";
    o << "\n";
    o << "    std::string toString() const {\n";
    o << "        if (esFinArchivo()) return \"EOF\";\n";
    o << "        std::string lp;\n";
    o << "        for (char c : lexema) {\n";
    o << "            if (c == '\\n') lp += \"\\\\n\";\n";
    o << "            else if (c == '\\t') lp += \"\\\\t\";\n";
    o << "            else if (c == '\\r') lp += \"\\\\r\";\n";
    o << "            else lp += c;\n";
    o << "        }\n";
    o << "        return std::string(TOKEN_TYPE_NAMES[tipo]) + \" '\" + lp + \"' [\" + std::to_string(linea) + \":\" + std::to_string(columna) + \"]\";\n";
    o << "    }\n";
    o << "};\n\n";

    // ── Tablas del AFD ──
    std::vector<std::string> syms(afd_min.alfabeto.begin(), afd_min.alfabeto.end());
    std::map<std::string, int> sym_idx;
    for (size_t i = 0; i < syms.size(); i++) sym_idx[syms[i]] = (int)i;

    o << "// ========== TABLAS DEL AFD MINIMIZADO ==========\n";
    o << "const int NUM_ESTADOS = " << afd_min.estados.size() << ";\n";
    o << "const int NUM_SIMBOLOS = " << syms.size() << ";\n";
    o << "const int ESTADO_INICIAL = " << afd_min.estado_inicial->id << ";\n\n";

    o << "static int char_to_sym[256];\n";
    o << "static int transicion[" << afd_min.estados.size() << "][" << syms.size() << "];\n";
    o << "static int token_aceptado[" << afd_min.estados.size() << "];\n";
    o << "static bool tablas_inicializadas = false;\n\n";

    o << "void init_tablas() {\n";
    o << "    if (tablas_inicializadas) return;\n";
    o << "    tablas_inicializadas = true;\n\n";
    o << "    for (int i = 0; i < 256; i++) char_to_sym[i] = -1;\n";
    for (size_t i = 0; i < syms.size(); i++) {
        if (syms[i].size() == 1) {
            int ascii = (int)(unsigned char)syms[i][0];
            o << "    char_to_sym[" << ascii << "] = " << i << ";";
            // Comentario con el carácter
            if (ascii == 9) o << " // \\t";
            else if (ascii == 10) o << " // \\n";
            else if (ascii == 13) o << " // \\r";
            else if (ascii == 32) o << " // espacio";
            else if (ascii >= 33 && ascii <= 126) o << " // '" << syms[i][0] << "'";
            o << "\n";
        }
    }

    o << "\n    for (int i = 0; i < NUM_ESTADOS; i++)\n";
    o << "        for (int j = 0; j < NUM_SIMBOLOS; j++)\n";
    o << "            transicion[i][j] = -1;\n\n";

    for (auto& up : afd_min.estados) {
        auto* e = up.get();
        for (auto& [s, d] : e->transiciones) {
            auto it = sym_idx.find(s);
            if (it != sym_idx.end()) {
                o << "    transicion[" << e->id << "][" << it->second << "] = " << d->id << ";\n";
            }
        }
    }

    o << "\n    for (int i = 0; i < NUM_ESTADOS; i++) token_aceptado[i] = -1;\n";
    for (auto& up : afd_min.estados) {
        auto* e = up.get();
        if (e->es_final && e->token_id >= 0) {
            o << "    token_aceptado[" << e->id << "] = " << e->token_id << ";";
            if (e->token_id < (int)nombres_token.size()) {
                o << " // " << nombres_token[e->token_id];
            }
            o << "\n";
        }
    }
    o << "}\n\n";

    // ── Clase Lexer ──
    o << "// ========== CLASE LEXER ==========\n";
    o << "class Lexer {\n";
    o << "private:\n";
    o << "    std::string entrada;\n";
    o << "    size_t pos;\n";
    o << "    int linea;\n";
    o << "    int columna;\n";
    o << "\n";
    o << "    bool esTokenSkip(int token_id) const {\n";
    o << "        std::string accion = TOKEN_ACTIONS[token_id];\n";
    o << "        // Tokens de whitespace/newline se saltan\n";
    o << "        return accion.find(\"return lexbuf\") != std::string::npos ||\n";
    o << "               accion.find(\"return EOL\") != std::string::npos;\n";
    o << "    }\n";
    o << "\n";
    o << "public:\n";
    o << "    Lexer() : pos(0), linea(1), columna(1) {\n";
    o << "        init_tablas();\n";
    o << "    }\n";
    o << "\n";
    o << "    Lexer(const std::string& texto) : entrada(texto), pos(0), linea(1), columna(1) {\n";
    o << "        init_tablas();\n";
    o << "    }\n";
    o << "\n";
    o << "    void setInput(const std::string& texto) {\n";
    o << "        entrada = texto;\n";
    o << "        pos = 0;\n";
    o << "        linea = 1;\n";
    o << "        columna = 1;\n";
    o << "    }\n";
    o << "\n";
    o << "    // Retorna el siguiente token.\n";
    o << "    // Al llegar al final retorna un token con tipo = (TokenType)-1\n";
    o << "    Token getNextToken() {\n";
    o << "        while (pos < entrada.size()) {\n";
    o << "            int estado_actual = ESTADO_INICIAL;\n";
    o << "            size_t inicio = pos;\n";
    o << "            int inicio_linea = linea;\n";
    o << "            int inicio_col = columna;\n";
    o << "            size_t ultimo_aceptado_pos = pos;\n";
    o << "            int ultimo_token = -1;\n";
    o << "\n";
    o << "            // Avanzar en el AFD buscando el lexema mas largo\n";
    o << "            size_t i = pos;\n";
    o << "            while (i < entrada.size()) {\n";
    o << "                int sym = char_to_sym[(unsigned char)entrada[i]];\n";
    o << "                if (sym < 0) break;\n";
    o << "                int siguiente = transicion[estado_actual][sym];\n";
    o << "                if (siguiente < 0) break;\n";
    o << "                estado_actual = siguiente;\n";
    o << "                i++;\n";
    o << "                if (token_aceptado[estado_actual] >= 0) {\n";
    o << "                    ultimo_aceptado_pos = i;\n";
    o << "                    ultimo_token = token_aceptado[estado_actual];\n";
    o << "                }\n";
    o << "            }\n";
    o << "\n";
    o << "            if (ultimo_token >= 0 && ultimo_aceptado_pos > pos) {\n";
    o << "                std::string lexema = entrada.substr(pos, ultimo_aceptado_pos - pos);\n";
    o << "\n";
    o << "                // Actualizar posicion\n";
    o << "                for (size_t k = pos; k < ultimo_aceptado_pos; k++) {\n";
    o << "                    if (entrada[k] == '\\n') { linea++; columna = 1; }\n";
    o << "                    else columna++;\n";
    o << "                }\n";
    o << "                pos = ultimo_aceptado_pos;\n";
    o << "\n";
    o << "                // Si es token de skip (whitespace), continuar al siguiente\n";
    o << "                if (esTokenSkip(ultimo_token)) continue;\n";
    o << "\n";
    o << "                // Retornar el token\n";
    o << "                return Token{(TokenType)ultimo_token, lexema, inicio_linea, inicio_col};\n";
    o << "            } else {\n";
    o << "                // Error lexico\n";
    o << "                std::cerr << \"ERROR LEXICO: '\" << entrada[pos]\n";
    o << "                          << \"' (ASCII \" << (int)(unsigned char)entrada[pos]\n";
    o << "                          << \") en linea \" << linea << \", columna \" << columna << std::endl;\n";
    o << "                if (entrada[pos] == '\\n') { linea++; columna = 1; }\n";
    o << "                else columna++;\n";
    o << "                pos++;\n";
    o << "            }\n";
    o << "        }\n";
    o << "\n";
    o << "        // Fin de archivo\n";
    o << "        return Token{(TokenType)-1, \"\", linea, columna};\n";
    o << "    }\n";
    o << "\n";
    o << "    // Tokeniza todo el input y retorna un vector de tokens\n";
    o << "    std::vector<Token> tokenizar() {\n";
    o << "        std::vector<Token> tokens;\n";
    o << "        while (true) {\n";
    o << "            Token tok = getNextToken();\n";
    o << "            if (tok.esFinArchivo()) break;\n";
    o << "            tokens.push_back(tok);\n";
    o << "        }\n";
    o << "        return tokens;\n";
    o << "    }\n";
    o << "\n";
    o << "    int getLinea() const { return linea; }\n";
    o << "    int getColumna() const { return columna; }\n";
    o << "    bool finArchivo() const { return pos >= entrada.size(); }\n";
    o << "};\n\n";

    // ── Main ──
    o << "// ========== MAIN ==========\n";
    o << "int main(int argc, char* argv[]) {\n";
    o << "    if (argc != 2) {\n";
    o << "        std::cerr << \"Uso: \" << argv[0] << \" <archivo_entrada>\" << std::endl;\n";
    o << "        return 1;\n";
    o << "    }\n";
    o << "\n";
    o << "    std::ifstream f(argv[1]);\n";
    o << "    if (!f.is_open()) {\n";
    o << "        std::cerr << \"Error: No se pudo abrir '\" << argv[1] << \"'\" << std::endl;\n";
    o << "        return 1;\n";
    o << "    }\n";
    o << "\n";
    o << "    std::ostringstream ss;\n";
    o << "    ss << f.rdbuf();\n";
    o << "    std::string entrada = ss.str();\n";
    o << "    f.close();\n";
    o << "\n";
    o << "    Lexer lexer(entrada);\n";
    o << "    std::vector<Token> tokens = lexer.tokenizar();\n";
    o << "\n";
    o << "    std::cout << \"=== ANALIZADOR LEXICO ===\" << std::endl;\n";
    o << "    std::cout << \"Archivo: \" << argv[1] << std::endl;\n";
    o << "    std::cout << \"Tokens encontrados: \" << tokens.size() << std::endl;\n";
    o << "    std::cout << std::endl;\n";
    o << "\n";
    o << "    for (const Token& tok : tokens) {\n";
    o << "        std::cout << tok.toString() << std::endl;\n";
    o << "    }\n";
    o << "\n";
    o << "    std::cout << std::endl << \"Analisis lexico completado.\" << std::endl;\n";
    o << "    return 0;\n";
    o << "}\n";

    // ── Trailer ──
    if (!yalex.trailer.empty()) {
        o << "\n// ========== TRAILER DEL USUARIO ==========\n";
        o << yalex.trailer << "\n";
        o << "// ========== FIN TRAILER ==========\n";
    }

    o.close();
    std::cout << "  Generado: " << nombre_salida << ".cpp\n";
}


// ╔══════════════════════════════════════════════════════════════╗
// ║  SECCIÓN 15 — MAIN                                         ║
// ╚══════════════════════════════════════════════════════════════╝

int main(int argc, char* argv[]) {
    if(argc<2){std::cerr<<"Uso: "<<argv[0]<<" <archivo.yal> [-o nombre_salida]\n";return 1;}
    std::string archivo_yal=argv[1], nombre_salida="lexer_generado";
    for(int i=2;i<argc;i++)if(std::string(argv[i])=="-o"&&i+1<argc){nombre_salida=argv[++i];}

    // Crear carpeta output/ para los .dot
    std::string dot_dir = "output";
    MKDIR(dot_dir.c_str());
    std::string dp = dot_dir + "/"; // prefijo para archivos dot

    std::cout<<"╔══════════════════════════════════════════════════════╗\n";
    std::cout<<"║   GENERADOR DE ANALIZADORES LÉXICOS                 ║\n";
    std::cout<<"╚══════════════════════════════════════════════════════╝\n\n";

    // ── FASE 1 ──
    ArchivoYalex yalex = parsear_yalex(archivo_yal);
    if(yalex.reglas.empty()){std::cerr<<"Error: No se encontraron reglas.\n";return 1;}
    std::cout<<"\n  Resumen Fase 1: "<<yalex.definiciones_raw.size()<<" defs, "<<yalex.reglas.size()<<" reglas, rule="<<yalex.nombre_regla<<"\n";

    // ── FASE 2 ──
    fase2_expandir_y_unificar(yalex);

    // ASTs individuales por regla
    std::cout<<"\n--- Árboles de expresión por regla ---\n";
    for(size_t i=0;i<yalex.reglas.size();i++){
        auto ast=regex_a_ast(yalex.reglas[i].regex_expandida);
        if(ast)dibujar_ast(ast,dp+"ast_regla_"+std::to_string(i),"Regla "+std::to_string(i)+": "+yalex.reglas[i].nombre_token);
    }

    // AST combinado
    std::cout<<"\n--- Árbol de expresión combinado (mega-regex) ---\n";
    {
        std::shared_ptr<NodoAST> combinado=nullptr;
        for(size_t i=0;i<yalex.reglas.size();i++){
            auto ast_i=regex_a_ast(yalex.reglas[i].regex_expandida);
            if(!ast_i)continue;
            auto marker=std::make_shared<NodoAST>("#T"+std::to_string(i)+"#");
            auto con_marker=std::make_shared<NodoAST>(".",ast_i,marker);
            if(!combinado)combinado=con_marker;
            else combinado=std::make_shared<NodoAST>("|",combinado,con_marker);
        }
        if(combinado)dibujar_ast(combinado,dp+"ast_combinado","Arbol de Expresion Combinado");
    }

    // ── FASE 3: Autómatas ──
    std::cout<<"\n╔══════════════════════════════════════════════════════╗\n";
    std::cout<<"║  CONSTRUCCIÓN DE AUTÓMATAS                          ║\n";
    std::cout<<"╚══════════════════════════════════════════════════════╝\n\n";

    AFN afn=construir_afn_combinado(yalex.reglas);
    std::cout<<"  AFN: "<<afn.estados.size()<<" estados, "<<afn.estados_finales.size()<<" finales, "<<afn.alfabeto.size()<<" símbolos\n";
    dibujar_afn(afn,dp+"afn_combinado"); std::cout<<"  "<<dp<<"afn_combinado.dot\n";

    AFD afd=construir_afd_subconjuntos(afn);
    std::cout<<"  AFD: "<<afd.estados.size()<<" estados\n";
    dibujar_afd(afd,dp+"afd_lexer"); std::cout<<"  "<<dp<<"afd_lexer.dot\n";

    AFD afd_min=minimizar_afd(afd);
    std::cout<<"  AFD min: "<<afd_min.estados.size()<<" estados";
    if(afd.estados.size()>0)std::cout<<" (reducción: "<<std::fixed<<std::setprecision(1)<<100.0*(double)(afd.estados.size()-afd_min.estados.size())/afd.estados.size()<<"%)";
    std::cout<<"\n";
    dibujar_afd(afd_min,dp+"afd_min_lexer"); std::cout<<"  "<<dp<<"afd_min_lexer.dot\n";

    // ── FASE 4: Generación de código ──
    std::cout<<"\n--- Generando analizador léxico ---\n";
    generar_analizador_lexico(yalex,afd_min,nombre_salida);

    std::cout<<"\n╔══════════════════════════════════════════════════════╗\n";
    std::cout<<"║  COMPLETADO                                         ║\n";
    std::cout<<"╚══════════════════════════════════════════════════════╝\n";
    std::cout<<"Archivos generados:\n";
    std::cout<<"  "<<nombre_salida<<".cpp  (analizador léxico)\n";
    std::cout<<"  "<<dot_dir<<"/         (archivos .dot)\n";
    std::cout<<"\nPara generar imágenes:\n";
    std::cout<<"  python visualizar.py "<<dot_dir<<"\n";
    return 0;
}