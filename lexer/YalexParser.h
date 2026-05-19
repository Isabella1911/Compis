#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>

#include "Token.h"

// ── Estructuras de automatas ──────────────────────────────────────────────────

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

// ── Estructuras de YalexParser ────────────────────────────────────────────────

struct ReglaLexica {
    std::string regex_original;
    std::string regex_expandida;
    std::string accion;
    int         prioridad;
    std::string nombre_token;
};

struct ArchivoYalex {
    std::string header;
    std::vector<std::pair<std::string, std::string>> definiciones_raw;
    std::map<std::string, std::string>               definiciones_expandidas;
    std::string                                       nombre_regla;
    std::vector<std::string>                          argumentos_regla;
    std::vector<ReglaLexica>                          reglas;
    std::string                                       trailer;
    std::string                                       mega_regex;
};

// ── Funciones publicas de YalexParser.cpp ────────────────────────────────────

ArchivoYalex    parsear_yalex(const std::string& ruta);
void            fase2_expandir_y_unificar(ArchivoYalex& yalex);
AFN             construir_afn_combinado(const std::vector<ReglaLexica>& reglas);
AFD             construir_afd_subconjuntos(AFN& afn);
AFD             minimizar_afd(AFD& afd);
ResultadoLexico ejecutar_lexer(AFD& afd_min, const ArchivoYalex& yalex,
                               const std::string& entrada);