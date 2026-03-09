/*
 Proyecto 1 - Implementacion ShuntingYard, Thompson (AFN), Subconjuntos (AFD), Minimizacion de AFD y Simulaciones
 Traducido a C++ desde Python
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


// NODO AST

struct NodoAST {
    std::string valor;
    std::shared_ptr<NodoAST> izq;
    std::shared_ptr<NodoAST> der;

    NodoAST(const std::string& v,
            std::shared_ptr<NodoAST> i = nullptr,
            std::shared_ptr<NodoAST> d = nullptr)
        : valor(v), izq(i), der(d) {}
};


// ESTADO AFN

struct EstadoAFN {
    int id;
    bool es_final;
    // simbolo -> lista de estados destino (punteros crudos, dueños los vectores del AFN)
    std::map<std::string, std::vector<EstadoAFN*>> transiciones;

    EstadoAFN(int id_estado) : id(id_estado), es_final(false) {}

    void agregar_transicion(const std::string& simbolo, EstadoAFN* destino) {
        transiciones[simbolo].push_back(destino);
    }
};


// AFN

struct AFN {
    EstadoAFN* estado_inicial = nullptr;
    std::vector<EstadoAFN*> estados_finales;
    std::vector<std::unique_ptr<EstadoAFN>> estados; // dueño de memoria
    std::set<std::string> alfabeto;
    int contador_estados = 0;
    AFN() = default;
    AFN(const AFN&) = delete;
    AFN& operator=(const AFN&) = delete;
    AFN(AFN&&) = default;
    AFN& operator=(AFN&&) = default;

    EstadoAFN* nuevo_estado() {
        auto e = std::make_unique<EstadoAFN>(contador_estados++);
        EstadoAFN* ptr = e.get();
        estados.push_back(std::move(e));
        return ptr;
    }

    void agregar_simbolo(const std::string& simbolo) {
        if (simbolo != "ε" && simbolo != "\xF0\x9D\x9C\x80")
            alfabeto.insert(simbolo);
    }
};


// FRAGMENTO AFN (para construcción de Thompson)

struct FragmentoAFN {
    EstadoAFN* inicio;
    EstadoAFN* fin;
    FragmentoAFN(EstadoAFN* i, EstadoAFN* f) : inicio(i), fin(f) {}
};


// ESTADO AFD

struct EstadoAFD {
    int id;
    std::set<int> conjunto_afn;          // IDs de los estados AFN que representa
    std::vector<EstadoAFN*> estados_afn; // punteros a los estados AFN (no dueño)
    bool es_final;
    std::map<std::string, EstadoAFD*> transiciones;

    EstadoAFD(int id_estado, const std::vector<EstadoAFN*>& conj)
        : id(id_estado), estados_afn(conj), es_final(false)
    {
        for (auto* e : conj) {
            conjunto_afn.insert(e->id);
            if (e->es_final) es_final = true;
        }
    }

    void agregar_transicion(const std::string& simbolo, EstadoAFD* destino) {
        transiciones[simbolo] = destino;
    }

    std::string repr() const {
        std::ostringstream ss;
        ss << "q" << id << "({";
        bool first = true;
        for (int x : conjunto_afn) { if (!first) ss << ","; ss << x; first = false; }
        ss << "})";
        return ss.str();
    }
};


// AFD

struct AFD {
    EstadoAFD* estado_inicial = nullptr;
    std::vector<EstadoAFD*> estados_finales;
    std::vector<std::unique_ptr<EstadoAFD>> estados;
    std::set<std::string> alfabeto;
    int contador_estados = 0;
    AFD() = default;
    AFD(const AFD&) = delete;
    AFD& operator=(const AFD&) = delete;
    AFD(AFD&&) = default;
    AFD& operator=(AFD&&) = default;

    EstadoAFD* nuevo_estado(const std::vector<EstadoAFN*>& conj) {
        auto e = std::make_unique<EstadoAFD>(contador_estados++, conj);
        EstadoAFD* ptr = e.get();
        estados.push_back(std::move(e));
        if (ptr->es_final) estados_finales.push_back(ptr);
        return ptr;
    }
};


// CLAUSURA EPSILON

std::set<EstadoAFN*> clausura_epsilon(const std::vector<EstadoAFN*>& estados_inicio) {
    std::set<EstadoAFN*> clausura(estados_inicio.begin(), estados_inicio.end());
    std::stack<EstadoAFN*> pila;
    for (auto* e : estados_inicio) pila.push(e);

    while (!pila.empty()) {
        EstadoAFN* actual = pila.top(); pila.pop();
        auto it = actual->transiciones.find("ε");
        if (it != actual->transiciones.end()) {
            for (EstadoAFN* dest : it->second) {
                if (clausura.find(dest) == clausura.end()) {
                    clausura.insert(dest);
                    pila.push(dest);
                }
            }
        }
    }
    return clausura;
}


// CONSTRUCCIÓN DE THOMPSON (AFN)

AFN construir_thompson_afn(std::shared_ptr<NodoAST> ast);

static const std::set<std::string> OPERADORES = {"|", ".", "*", "+", "?", "^"};

AFN construir_thompson_afn(std::shared_ptr<NodoAST> ast) {
    AFN afn;
    if (!ast) return afn;

    std::function<FragmentoAFN(std::shared_ptr<NodoAST>)> construir =
        [&](std::shared_ptr<NodoAST> nodo) -> FragmentoAFN {

        if (OPERADORES.find(nodo->valor) == OPERADORES.end()) {
            // Literal
            EstadoAFN* inicio = afn.nuevo_estado();
            EstadoAFN* fin   = afn.nuevo_estado();

            std::string simbolo = nodo->valor;
            if (!simbolo.empty() && simbolo[0] == '\\')
                simbolo = simbolo.substr(1);

            if (simbolo == "ε") {
                inicio->agregar_transicion("ε", fin);
            } else {
                afn.agregar_simbolo(simbolo);
                inicio->agregar_transicion(simbolo, fin);
            }
            return FragmentoAFN(inicio, fin);
        }

        if (nodo->valor == ".") {
            FragmentoAFN frag_izq = construir(nodo->izq);
            FragmentoAFN frag_der = construir(nodo->der);
            frag_izq.fin->agregar_transicion("ε", frag_der.inicio);
            return FragmentoAFN(frag_izq.inicio, frag_der.fin);
        }

        if (nodo->valor == "|") {
            FragmentoAFN frag_izq = construir(nodo->izq);
            FragmentoAFN frag_der = construir(nodo->der);
            EstadoAFN* inicio = afn.nuevo_estado();
            EstadoAFN* fin   = afn.nuevo_estado();
            inicio->agregar_transicion("ε", frag_izq.inicio);
            inicio->agregar_transicion("ε", frag_der.inicio);
            frag_izq.fin->agregar_transicion("ε", fin);
            frag_der.fin->agregar_transicion("ε", fin);
            return FragmentoAFN(inicio, fin);
        }

        if (nodo->valor == "*") {
            FragmentoAFN frag = construir(nodo->izq);
            EstadoAFN* inicio = afn.nuevo_estado();
            EstadoAFN* fin   = afn.nuevo_estado();
            inicio->agregar_transicion("ε", frag.inicio);
            inicio->agregar_transicion("ε", fin);
            frag.fin->agregar_transicion("ε", frag.inicio);
            frag.fin->agregar_transicion("ε", fin);
            return FragmentoAFN(inicio, fin);
        }

        if (nodo->valor == "+") {
            FragmentoAFN frag = construir(nodo->izq);
            EstadoAFN* inicio = afn.nuevo_estado();
            EstadoAFN* fin   = afn.nuevo_estado();
            inicio->agregar_transicion("ε", frag.inicio);
            frag.fin->agregar_transicion("ε", frag.inicio);
            frag.fin->agregar_transicion("ε", fin);
            return FragmentoAFN(inicio, fin);
        }

        if (nodo->valor == "?") {
            FragmentoAFN frag = construir(nodo->izq);
            EstadoAFN* inicio = afn.nuevo_estado();
            EstadoAFN* fin   = afn.nuevo_estado();
            inicio->agregar_transicion("ε", frag.inicio);
            inicio->agregar_transicion("ε", fin);
            frag.fin->agregar_transicion("ε", fin);
            return FragmentoAFN(inicio, fin);
        }

        // Fallback: tratar como literal
        EstadoAFN* inicio = afn.nuevo_estado();
        EstadoAFN* fin   = afn.nuevo_estado();
        inicio->agregar_transicion(nodo->valor, fin);
        afn.agregar_simbolo(nodo->valor);
        return FragmentoAFN(inicio, fin);
    };

    FragmentoAFN frag = construir(ast);
    afn.estado_inicial = frag.inicio;
    frag.fin->es_final = true;
    afn.estados_finales.push_back(frag.fin);

    return afn;
}


// CONSTRUCCIÓN DE SUBCONJUNTOS (AFN -> AFD)

AFD construir_afd_subconjuntos(AFN& afn) {
    AFD afd;
    afd.alfabeto = afn.alfabeto;

    std::set<EstadoAFN*> clausura_ini = clausura_epsilon({afn.estado_inicial});
    std::vector<EstadoAFN*> conj_ini(clausura_ini.begin(), clausura_ini.end());
    EstadoAFD* estado_ini_afd = afd.nuevo_estado(conj_ini);
    afd.estado_inicial = estado_ini_afd;

    std::cout << "\n=== CONSTRUCCIÓN DE SUBCONJUNTOS (AFN -> AFD) ===\n";
    std::cout << "Estado Inicial del AFD: " << estado_ini_afd->repr() << "\n";

    // Mapa: conjunto de IDs AFN -> puntero EstadoAFD
    std::map<std::set<int>, EstadoAFD*> visitados;
    visitados[estado_ini_afd->conjunto_afn] = estado_ini_afd;

    std::queue<EstadoAFD*> por_procesar;
    por_procesar.push(estado_ini_afd);

    while (!por_procesar.empty()) {
        EstadoAFD* actual = por_procesar.front(); por_procesar.pop();
        std::cout << "\nProcesando estado: " << actual->repr() << "\n";

        for (const std::string& simbolo : afd.alfabeto) {
            std::set<EstadoAFN*> destinos;
            for (EstadoAFN* e_afn : actual->estados_afn) {
                auto it = e_afn->transiciones.find(simbolo);
                if (it != e_afn->transiciones.end()) {
                    for (EstadoAFN* d : it->second)
                        destinos.insert(d);
                }
            }

            if (destinos.empty()) continue;

            std::vector<EstadoAFN*> dest_vec(destinos.begin(), destinos.end());
            std::set<EstadoAFN*> clausura_dest = clausura_epsilon(dest_vec);
            std::vector<EstadoAFN*> clausura_vec(clausura_dest.begin(), clausura_dest.end());

            std::set<int> ids;
            for (auto* e : clausura_vec) ids.insert(e->id);

            EstadoAFD* dest_afd;
            auto it = visitados.find(ids);
            if (it != visitados.end()) {
                dest_afd = it->second;
            } else {
                dest_afd = afd.nuevo_estado(clausura_vec);
                visitados[ids] = dest_afd;
                por_procesar.push(dest_afd);
                std::cout << "  Nuevo estado creado: " << dest_afd->repr() << "\n";
            }

            actual->agregar_transicion(simbolo, dest_afd);
            std::cout << "  Transición: " << actual->repr()
                      << " --" << simbolo << "--> " << dest_afd->repr() << "\n";
        }
    }

    std::cout << "\n=== AFD CONSTRUIDO ===\n";
    std::cout << "Estados totales: " << afd.estados.size() << "\n";
    std::cout << "Estados finales: " << afd.estados_finales.size() << "\n";
    return afd;
}


// MINIMIZACIÓN DEL AFD


// Eliminar estados inalcanzables
AFD eliminar_estados_inalcanzables(AFD& afd) {
    std::set<EstadoAFD*> alcanzables;
    std::queue<EstadoAFD*> cola;
    alcanzables.insert(afd.estado_inicial);
    cola.push(afd.estado_inicial);

    while (!cola.empty()) {
        EstadoAFD* actual = cola.front(); cola.pop();
        for (auto& [sim, dest] : actual->transiciones) {
            if (alcanzables.find(dest) == alcanzables.end()) {
                alcanzables.insert(dest);
                cola.push(dest);
            }
        }
    }

    AFD afd_limpio;
    afd_limpio.alfabeto = afd.alfabeto;

    std::map<EstadoAFD*, EstadoAFD*> mapeo;
    for (EstadoAFD* e : alcanzables) {
        EstadoAFD* nuevo = afd_limpio.nuevo_estado(e->estados_afn);
        nuevo->es_final = e->es_final;
        mapeo[e] = nuevo;
        if (nuevo->es_final) {
            // ya fue agregado por nuevo_estado si es_final, pero nuevo_estado lee
            // el conjunto que viene del vector original; re-asignamos manualmente
        }
    }
    // Reconstruir finales
    afd_limpio.estados_finales.clear();
    for (auto& [viejo, nuevo] : mapeo) {
        if (nuevo->es_final) afd_limpio.estados_finales.push_back(nuevo);
    }

    afd_limpio.estado_inicial = mapeo[afd.estado_inicial];

    for (auto& [viejo, nuevo] : mapeo) {
        for (auto& [sim, dest] : viejo->transiciones) {
            auto it = mapeo.find(dest);
            if (it != mapeo.end())
                nuevo->transiciones[sim] = it->second;
        }
    }

    afd_limpio.contador_estados = (int)afd_limpio.estados.size();
    return afd_limpio;
}

// Eliminar estados muertos
AFD eliminar_estados_muertos(AFD& afd) {
    std::set<EstadoAFD*> vivos(afd.estados_finales.begin(), afd.estados_finales.end());
    bool cambio = true;
    while (cambio) {
        cambio = false;
        for (auto& up : afd.estados) {
            EstadoAFD* e = up.get();
            if (vivos.find(e) != vivos.end()) continue;
            for (auto& [sim, dest] : e->transiciones) {
                if (vivos.find(dest) != vivos.end()) {
                    vivos.insert(e);
                    cambio = true;
                    break;
                }
            }
        }
    }

    if (vivos.find(afd.estado_inicial) == vivos.end()) {
        // Lenguaje vacío
        AFD vacio;
        vacio.alfabeto = afd.alfabeto;
        vacio.estado_inicial = vacio.nuevo_estado({});
        return vacio;
    }

    AFD afd_limpio;
    afd_limpio.alfabeto = afd.alfabeto;
    std::map<EstadoAFD*, EstadoAFD*> mapeo;

    for (EstadoAFD* e : vivos) {
        EstadoAFD* nuevo = afd_limpio.nuevo_estado(e->estados_afn);
        nuevo->es_final = e->es_final;
        mapeo[e] = nuevo;
    }

    afd_limpio.estados_finales.clear();
    for (auto& [viejo, nuevo] : mapeo)
        if (nuevo->es_final) afd_limpio.estados_finales.push_back(nuevo);

    afd_limpio.estado_inicial = mapeo[afd.estado_inicial];

    for (auto& [viejo, nuevo] : mapeo) {
        for (auto& [sim, dest] : viejo->transiciones) {
            auto it = mapeo.find(dest);
            if (it != mapeo.end())
                nuevo->transiciones[sim] = it->second;
        }
    }

    afd_limpio.contador_estados = (int)afd_limpio.estados.size();
    return afd_limpio;
}

// Obtener el índice de partición a la que pertenece un estado
static int particion_de(EstadoAFD* e,
                         const std::vector<std::set<EstadoAFD*>>& particiones) {
    for (int i = 0; i < (int)particiones.size(); ++i)
        if (particiones[i].count(e)) return i;
    return -1;
}

// Dividir una partición
std::vector<std::set<EstadoAFD*>> dividir_particion(
    const std::set<EstadoAFD*>& particion,
    const std::vector<std::set<EstadoAFD*>>& todas,
    const std::set<std::string>& alfabeto)
{
    if (particion.size() <= 1)
        return {particion};

    std::map<std::vector<std::pair<std::string,int>>, std::set<EstadoAFD*>> grupos;

    for (EstadoAFD* e : particion) {
        std::vector<std::pair<std::string,int>> firma;
        for (const std::string& sim : alfabeto) {
            auto it = e->transiciones.find(sim);
            if (it != e->transiciones.end())
                firma.push_back({sim, particion_de(it->second, todas)});
            else
                firma.push_back({sim, -1});
        }
        grupos[firma].insert(e);
    }

    std::vector<std::set<EstadoAFD*>> result;
    for (auto& [k, v] : grupos) result.push_back(v);
    return result;
}

// Construir el AFD minimizado a partir de particiones
AFD construir_afd_minimizado(AFD& afd_original,
                              const std::vector<std::set<EstadoAFD*>>& particiones) {
    AFD afd_min;
    afd_min.alfabeto = afd_original.alfabeto;

    // mapeo: partición (por índice) -> nuevo EstadoAFD*
    std::vector<EstadoAFD*> estados_min(particiones.size(), nullptr);

    for (int i = 0; i < (int)particiones.size(); ++i) {
        const std::set<EstadoAFD*>& part = particiones[i];
        EstadoAFD* repr = *part.begin();

        // Combinar conjuntos AFN
        std::set<int> conjunto_combinado;
        for (EstadoAFD* e : part)
            for (int id : e->conjunto_afn)
                conjunto_combinado.insert(id);

        auto nuevo = std::make_unique<EstadoAFD>(i, std::vector<EstadoAFN*>{});
        nuevo->conjunto_afn = conjunto_combinado;
        nuevo->es_final = repr->es_final;

        EstadoAFD* ptr = nuevo.get();
        afd_min.estados.push_back(std::move(nuevo));
        estados_min[i] = ptr;

        if (ptr->es_final) afd_min.estados_finales.push_back(ptr);

        if (part.count(afd_original.estado_inicial))
            afd_min.estado_inicial = ptr;
    }

    // Transiciones
    for (int i = 0; i < (int)particiones.size(); ++i) {
        EstadoAFD* repr = *particiones[i].begin();
        for (const std::string& sim : afd_min.alfabeto) {
            auto it = repr->transiciones.find(sim);
            if (it == repr->transiciones.end()) continue;
            EstadoAFD* dest_orig = it->second;
            int j = particion_de(dest_orig, particiones);
            if (j >= 0) estados_min[i]->agregar_transicion(sim, estados_min[j]);
        }
    }

    afd_min.contador_estados = (int)afd_min.estados.size();
    return afd_min;
}

// Algoritmo de particionamiento (Hopcroft simplificado)
AFD algoritmo_particionamiento(AFD& afd) {
    if (afd.estados.empty()) return construir_afd_minimizado(afd, {});

    std::cout << "ALGORITMO DE PARTICIONAMIENTO\n";

    std::set<EstadoAFD*> finales(afd.estados_finales.begin(), afd.estados_finales.end());
    std::set<EstadoAFD*> no_finales;
    for (auto& up : afd.estados)
        if (!finales.count(up.get())) no_finales.insert(up.get());

    std::vector<std::set<EstadoAFD*>> particiones;
    if (!finales.empty())    particiones.push_back(finales);
    if (!no_finales.empty()) particiones.push_back(no_finales);

    bool hubo_cambio = true;
    while (hubo_cambio) {
        hubo_cambio = false;
        std::vector<std::set<EstadoAFD*>> nuevas;
        for (auto& part : particiones) {
            auto subs = dividir_particion(part, particiones, afd.alfabeto);
            if (subs.size() > 1) hubo_cambio = true;
            for (auto& s : subs) nuevas.push_back(s);
        }
        particiones = nuevas;
    }

    return construir_afd_minimizado(afd, particiones);
}

AFD minimizar_afd(AFD& afd) {
    std::cout << "=== MINIMIZACION DEL AFD ===\n";
    AFD sin_inalcanzables = eliminar_estados_inalcanzables(afd);
    AFD sin_muertos       = eliminar_estados_muertos(sin_inalcanzables);
    AFD minimizado        = algoritmo_particionamiento(sin_muertos);
    std::cout << "AFD minimizado tiene " << minimizado.estados.size() << " estados\n";
    std::cout << "Estados finales minimizados: " << minimizado.estados_finales.size() << "\n";
    return minimizado;
}


// SIMULACIONES

bool simular_afd(AFD& afd, const std::string& cadena) {
    if (!afd.estado_inicial) return false;
    EstadoAFD* actual = afd.estado_inicial;
    std::cout << "Estado inicial AFD: " << actual->repr() << "\n";

    for (char ch : cadena) {
        std::string sim(1, ch);
        auto it = actual->transiciones.find(sim);
        if (it == actual->transiciones.end()) {
            std::cout << "No hay transición con '" << sim << "' desde " << actual->repr() << "\n";
            return false;
        }
        actual = it->second;
        std::cout << "Después de '" << sim << "': " << actual->repr() << "\n";
    }
    return actual->es_final;
}

bool simular_afn(AFN& afn, const std::string& cadena) {
    if (!afn.estado_inicial) return false;

    std::set<EstadoAFN*> actuales = clausura_epsilon({afn.estado_inicial});

    std::cout << "Estado inicial: [";
    bool f = true;
    for (auto* e : actuales) { if (!f) std::cout << ","; std::cout << e->id; f = false; }
    std::cout << "]\n";

    for (char ch : cadena) {
        std::string sim(1, ch);
        std::set<EstadoAFN*> nuevos;
        for (EstadoAFN* e : actuales) {
            auto it = e->transiciones.find(sim);
            if (it != e->transiciones.end())
                for (EstadoAFN* d : it->second)
                    nuevos.insert(d);
        }
        if (!nuevos.empty()) {
            std::vector<EstadoAFN*> v(nuevos.begin(), nuevos.end());
            actuales = clausura_epsilon(v);
        } else {
            actuales.clear();
        }

        std::cout << "Después de '" << sim << "': [";
        bool ff = true;
        for (auto* e : actuales) { if (!ff) std::cout << ","; std::cout << e->id; ff = false; }
        std::cout << "]\n";

        if (actuales.empty()) return false;
    }

    for (EstadoAFN* e : actuales)
        if (e->es_final) return true;
    return false;
}


// VISUALIZACIÓN CON GRAPHVIZ (genera archivos .dot)

void dibujar_ast(std::shared_ptr<NodoAST> raiz, const std::string& nombre) {
    std::ofstream f(nombre + ".dot");
    f << "digraph AST {\n  rankdir=TB;\n  ranksep=1.0;\n";

    int contador = 0;
    std::function<int(std::shared_ptr<NodoAST>)> agregar = [&](std::shared_ptr<NodoAST> nodo) -> int {
        if (!nodo) return -1;
        int id = contador++;
        std::string etiqueta = nodo->valor;
        // Escapar caracteres especiales para DOT
        std::string safe;
        for (char c : etiqueta) {
            if (c == '"' || c == '\\') safe += '\\';
            safe += c;
        }
        f << "  " << id << " [label=\"" << safe << "\"];\n";
        int izq_id = agregar(nodo->izq);
        if (izq_id >= 0) f << "  " << id << " -> " << izq_id << ";\n";
        int der_id = agregar(nodo->der);
        if (der_id >= 0) f << "  " << id << " -> " << der_id << ";\n";
        return id;
    };
    agregar(raiz);
    f << "}\n";
    std::string cmd = "dot -Tpng " + nombre + ".dot -o " + nombre + ".png 2>/dev/null";
    system(cmd.c_str());
    std::cout << "Árbol guardado como: " << nombre << ".png\n";
}

void dibujar_afn(AFN& afn, const std::string& nombre) {
    std::ofstream f(nombre + ".dot");
    f << "digraph AFN {\n  rankdir=LR;\n  ranksep=1.0;\n";
    f << "  inicio [shape=none, label=\"\", width=0, height=0];\n";

    for (auto& up : afn.estados) {
        EstadoAFN* e = up.get();
        std::string shape = e->es_final ? "doublecircle" : "circle";
        f << "  " << e->id << " [shape=" << shape << ", label=\"" << e->id << "\"];\n";
    }
    if (afn.estado_inicial)
        f << "  inicio -> " << afn.estado_inicial->id << ";\n";

    for (auto& up : afn.estados) {
        EstadoAFN* e = up.get();
        for (auto& [sim, dests] : e->transiciones) {
            for (EstadoAFN* d : dests) {
                std::string lbl = (sim == "ε") ? "ε" : sim;
                f << "  " << e->id << " -> " << d->id << " [label=\"" << lbl << "\"];\n";
            }
        }
    }
    f << "}\n";
    std::string cmd = "dot -Tpng " + nombre + ".dot -o " + nombre + ".png 2>/dev/null";
    system(cmd.c_str());
    std::cout << "AFN guardado como: " << nombre << ".png\n";
}

void dibujar_afd(AFD& afd, const std::string& nombre) {
    std::ofstream f(nombre + ".dot");
    f << "digraph AFD {\n  rankdir=LR;\n  ranksep=1.5;\n";
    f << "  inicio_afd [shape=none, label=\"\", width=0, height=0];\n";

    for (auto& up : afd.estados) {
        EstadoAFD* e = up.get();
        std::string ids_str;
        bool first = true;
        for (int id : e->conjunto_afn) {
            if (!first) ids_str += ",";
            ids_str += std::to_string(id);
            first = false;
        }
        std::string etiqueta = "q" + std::to_string(e->id) + "\\n{" + ids_str + "}";
        std::string shape = e->es_final ? "doublecircle" : "circle";
        f << "  " << e->id << " [shape=" << shape << ", label=\"" << etiqueta << "\"];\n";
    }
    if (afd.estado_inicial)
        f << "  inicio_afd -> " << afd.estado_inicial->id << ";\n";

    for (auto& up : afd.estados) {
        EstadoAFD* e = up.get();
        for (auto& [sim, dest] : e->transiciones)
            f << "  " << e->id << " -> " << dest->id << " [label=\"" << sim << "\"];\n";
    }
    f << "}\n";
    std::string cmd = "dot -Tpng " + nombre + ".dot -o " + nombre + ".png 2>/dev/null";
    system(cmd.c_str());
    std::cout << "AFD guardado como: " << nombre << ".png\n";
}

void dibujar_afd_minimizado(AFD& afd, const std::string& nombre) {
    std::ofstream f(nombre + ".dot");
    f << "digraph AFDMin {\n  rankdir=LR;\n  ranksep=1.5;\n";
    f << "  node [style=filled, fillcolor=lightblue];\n";
    f << "  inicio_min [shape=none, label=\"\", width=0, height=0, style=invis];\n";

    for (auto& up : afd.estados) {
        EstadoAFD* e = up.get();
        std::string etiqueta = "q" + std::to_string(e->id) + "'";
        if (e->es_final)
            f << "  " << e->id << " [shape=doublecircle, label=\"" << etiqueta << "\", fillcolor=lightgreen];\n";
        else
            f << "  " << e->id << " [shape=circle, label=\"" << etiqueta << "\"];\n";
    }
    if (afd.estado_inicial)
        f << "  inicio_min -> " << afd.estado_inicial->id << ";\n";

    for (auto& up : afd.estados) {
        EstadoAFD* e = up.get();
        for (auto& [sim, dest] : e->transiciones)
            f << "  " << e->id << " -> " << dest->id << " [label=\"" << sim << "\"];\n";
    }
    f << "}\n";
    std::string cmd = "dot -Tpng " + nombre + ".dot -o " + nombre + ".png 2>/dev/null";
    system(cmd.c_str());
    std::cout << "AFD Minimizado guardado como: " << nombre << ".png\n";
}


// SHUNTING YARD + AST

int get_precedencia(const std::string& c) {
    if (c == "(") return 1;
    if (c == "|") return 2;
    if (c == ".") return 3;
    if (c == "?" || c == "*" || c == "+") return 4;
    if (c == "^") return 5;
    return 0;
}

// format_regex: inserta puntos de concatenación explícitos
std::vector<std::string> format_regex(const std::string& regex) {
    std::vector<std::string> res;
    size_t i = 0;

    auto unicode_op = [](unsigned char c, unsigned char c2, unsigned char c3) -> std::string {
        // UTF-8 detection for ∗ ？ ＋
        return "";
    };

    while (i < regex.size()) {
        // Detectar escape
        std::string c1;
        if (regex[i] == '\\' && i + 1 < regex.size()) {
            c1 = regex.substr(i, 2);
            i += 2;
        } else {
            // Posible UTF-8 multi-byte
            unsigned char uc = (unsigned char)regex[i];
            int bytes = 1;
            if ((uc & 0xE0) == 0xC0) bytes = 2;
            else if ((uc & 0xF0) == 0xE0) bytes = 3;
            else if ((uc & 0xF8) == 0xF0) bytes = 4;
            c1 = regex.substr(i, bytes);
            i += bytes;

            // Normalizar operadores Unicode
            if (c1 == "∗") c1 = "*";
            else if (c1 == "？") c1 = "?";
            else if (c1 == "＋") c1 = "+";
        }
        res.push_back(c1);

        if (i < regex.size()) {
            std::string c2;
            unsigned char uc2 = (unsigned char)regex[i];
            int bytes2 = 1;
            if ((uc2 & 0xE0) == 0xC0) bytes2 = 2;
            else if ((uc2 & 0xF0) == 0xE0) bytes2 = 3;
            else if ((uc2 & 0xF8) == 0xF0) bytes2 = 4;
            c2 = regex.substr(i, bytes2);

            if (c2 == "∗") c2 = "*";
            else if (c2 == "？") c2 = "?";
            else if (c2 == "＋") c2 = "+";

            static const std::set<std::string> NO_DOT_AFTER  = {"(", "|"};
            static const std::set<std::string> NO_DOT_BEFORE = {")", "|", "?", "*", "+", "^"};
            static const std::set<std::string> NO_DOT_IF_C1  = {"?", "*", "+"};

            bool c1_no_after  = NO_DOT_AFTER.count(c1) > 0;
            bool c2_no_before = NO_DOT_BEFORE.count(c2) > 0;
            bool c1_is_unary  = NO_DOT_IF_C1.count(c1) > 0;

            if (!c1_no_after && !c2_no_before && !c1_is_unary)
                res.push_back(".");
        }
    }
    return res;
}

// construir_AST desde postfix (lista de tokens)
std::shared_ptr<NodoAST> construir_AST(const std::vector<std::string>& tokens) {
    std::stack<std::shared_ptr<NodoAST>> pila;

    for (const std::string& token : tokens) {
        if (token == "|" || token == "." || token == "^") {
            if (pila.size() < 2) break;
            auto der = pila.top(); pila.pop();
            auto izq = pila.top(); pila.pop();
            pila.push(std::make_shared<NodoAST>(token, izq, der));
        } else if (token == "*" || token == "+" || token == "?") {
            if (pila.empty()) break;
            auto hijo = pila.top(); pila.pop();
            pila.push(std::make_shared<NodoAST>(token, hijo));
        } else {
            pila.push(std::make_shared<NodoAST>(token));
        }
    }

    if (pila.size() == 1) return pila.top();
    while (pila.size() > 1) {
        auto der = pila.top(); pila.pop();
        auto izq = pila.top(); pila.pop();
        pila.push(std::make_shared<NodoAST>(".", izq, der));
    }
    return pila.empty() ? nullptr : pila.top();
}

// Estructura de resultado del procesamiento de una línea
struct ResultadoLinea {
    std::shared_ptr<AFN> afn;
    std::shared_ptr<AFD> afd;
    std::shared_ptr<AFD> afd_min;
};

ResultadoLinea infix_to_postfix(const std::string& regex) {
    ResultadoLinea res;

    std::vector<std::string> formatted = format_regex(regex);

    std::cout << "Infix original: " << regex << "\n";
    std::cout << "Infix formateado: ";
    for (auto& t : formatted) std::cout << t;
    std::cout << "\n\n";

    std::vector<std::string> output;
    std::stack<std::string> stack;

    static const std::set<std::string> OPERADORES_ALL = {"|", ".", "(", ")", "?", "*", "+", "^"};

    for (const std::string& token : formatted) {
        bool es_literal = token.size() > 1 || OPERADORES_ALL.find(token) == OPERADORES_ALL.end();
        bool es_escape  = (token.size() >= 2 && token[0] == '\\');

        if (es_escape || es_literal) {
            output.push_back(token);
            std::cout << " Salida <- " << token << " Pila: [";
            std::stack<std::string> tmp = stack;
            std::vector<std::string> pv;
            while (!tmp.empty()) { pv.push_back(tmp.top()); tmp.pop(); }
            std::reverse(pv.begin(), pv.end());
            for (auto& x : pv) std::cout << x;
            std::cout << "]\n";
            continue;
        }

        if (token == "*" || token == "+" || token == "?") {
            output.push_back(token);
            continue;
        }

        if (token == "(") {
            stack.push(token);
            continue;
        }

        if (token == ")") {
            while (!stack.empty() && stack.top() != "(") {
                output.push_back(stack.top()); stack.pop();
            }
            if (!stack.empty()) stack.pop(); // descartar '('
            continue;
        }

        int prec = get_precedencia(token);
        while (!stack.empty() && get_precedencia(stack.top()) >= prec) {
            output.push_back(stack.top()); stack.pop();
        }
        stack.push(token);
    }

    while (!stack.empty()) {
        output.push_back(stack.top()); stack.pop();
    }

    std::cout << "\n Postfix resultante: ";
    for (auto& t : output) std::cout << t << " ";
    std::cout << "\n";

    std::cout << "Construyendo AST\n";
    auto ast = construir_AST(output);

    if (ast) {
        // Nombre base sanitizado
        std::string nombre_base = regex;
        for (char& c : nombre_base)
            if (c == '*' || c == '|' || c == '(' || c == ')' || c == '?' || c == '+')
                c = '_';

        dibujar_ast(ast, "ast_linea_" + nombre_base);

        std::cout << "Construyendo AFN con el algoritmo de Thompson...\n";
        auto afn_ptr = std::make_shared<AFN>(std::move(construir_thompson_afn(ast)));
        if (afn_ptr->estado_inicial) {
            dibujar_afn(*afn_ptr, "afn_linea_" + nombre_base);
            std::cout << "AFN construido con " << afn_ptr->estados.size() << " estados\n";

            std::cout << "Construyendo AFD con algoritmo de subconjuntos...\n";
            auto afd_ptr = std::make_shared<AFD>(std::move(construir_afd_subconjuntos(*afn_ptr)));
            dibujar_afd(*afd_ptr, "afd_linea_" + nombre_base);

            std::cout << "Minimizando AFD...\n";
            auto afd_min_ptr = std::make_shared<AFD>(std::move(minimizar_afd(*afd_ptr)));
            dibujar_afd_minimizado(*afd_min_ptr, "afd_min_linea_" + nombre_base);
            std::cout << "AFD minimizado con " << afd_min_ptr->estados.size() << " estados\n";

            res.afn     = afn_ptr;
            res.afd     = afd_ptr;
            res.afd_min = afd_min_ptr;
        }
    }
    std::cout << "\n";
    return res;
}


// MAIN

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <archivo_expresiones>\n";
        return 1;
    }

    std::ifstream archivo(argv[1]);
    if (!archivo.is_open()) {
        std::cerr << "No se encontró el archivo: " << argv[1] << "\n";
        return 1;
    }

    std::vector<std::shared_ptr<AFN>> afns;
    std::vector<std::shared_ptr<AFD>> afds;
    std::vector<std::shared_ptr<AFD>> afds_min;
    std::vector<std::string> expresiones;

    std::string linea;
    int idx = 1;
    while (std::getline(archivo, linea)) {
        // Quitar \r si existe
        if (!linea.empty() && linea.back() == '\r') linea.pop_back();
        if (linea.empty()) continue;

        std::cout << "=== Línea " << idx++ << " ===\n";
        ResultadoLinea res = infix_to_postfix(linea);
        if (res.afn) {
            afns.push_back(res.afn);
            afds.push_back(res.afd);
            afds_min.push_back(res.afd_min);
            expresiones.push_back(linea);
        }
    }

    if (afns.empty()) {
        std::cout << "No se generaron AFNs válidos.\n";
        return 0;
    }

    std::cout << "================\n";
    std::cout << "SIMULACION DE AUTOMATAS\n";
    std::cout << "================\n";

    while (true) {
        std::cout << "===== AUTOMATAS Disponibles ====\n";
        for (int i = 0; i < (int)expresiones.size(); ++i)
            std::cout << (i + 1) << ". " << expresiones[i] << "\n";
        std::cout << "0. Salir\n";

        int opcion;
        std::cout << "Selecciona el autómata a usar (numero): ";
        if (!(std::cin >> opcion)) break;

        if (opcion == 0) {
            std::cout << "Gracias por probar la simulacion\n";
            break;
        }
        if (opcion < 1 || opcion > (int)expresiones.size()) {
            std::cout << "Opción no válida. Intenta de nuevo\n";
            continue;
        }

        int idx_sel = opcion - 1;
        AFN& afn_sel     = *afns[idx_sel];
        AFD& afd_sel     = *afds[idx_sel];
        AFD& afd_min_sel = *afds_min[idx_sel];
        const std::string& expr = expresiones[idx_sel];

        std::string cadena;
        std::cout << "Escribe la cadena a evaluar: ";
        std::cin >> cadena;

        std::cout << "=========\n";
        std::cout << "Evaluando: '" << cadena << "' en L(" << expr << ")\n";
        std::cout << "=========\n";

        // AFN
        std::cout << "\n--- SIMULACIÓN AFN ---\n";
        bool res_afn = simular_afn(afn_sel, cadena);
        std::cout << "AFN: ¿'" << cadena << "' ∈ L(" << expr << ")? " << (res_afn ? "SI" : "NO") << "\n";

        // AFD
        std::cout << "\n--- SIMULACIÓN AFD ---\n";
        bool res_afd = simular_afd(afd_sel, cadena);
        std::cout << "AFD: ¿'" << cadena << "' ∈ L(" << expr << ")? " << (res_afd ? "SI" : "NO") << "\n";

        // AFD Minimizado
        std::cout << "\n--- SIMULACIÓN AFD MINIMIZADO ---\n";
        bool res_min = simular_afd(afd_min_sel, cadena);
        std::cout << "AFD Minimizado: ¿'" << cadena << "' ∈ L(" << expr << ")? " << (res_min ? "SI" : "NO") << "\n";

        std::cout << "=====================\n";
        std::cout << "Verificacion de resultados:\n";
        std::cout << "=====================\n";

        if (res_afn == res_afd && res_afd == res_min) {
            std::cout << "Todos los automatas dan el mismo resultado\n";
            std::cout << "Resultado final: " << (res_afn ? "SI" : "NO") << "\n";
        } else {
            std::cout << "Error, Los automatas dan resultados diferentes\n";
            std::cout << "AFN: "           << (res_afn ? "SI" : "NO") << "\n";
            std::cout << "AFD: "           << (res_afd ? "SI" : "NO") << "\n";
            std::cout << "AFD Min: "       << (res_min ? "SI" : "NO") << "\n";
        }

        std::cout << "=====================\n";
        std::cout << "RESUMEN FINAL DE LOS AUTOMATAS:\n";
        std::cout << "=====================\n";
        std::cout << "AFN: " << afn_sel.estados.size() << " estados\n";
        std::cout << "AFD: " << afd_sel.estados.size() << " estados\n";
        std::cout << "AFD Minimizado: " << afd_min_sel.estados.size() << " estados\n";

        if (afd_sel.estados.size() > 0) {
            double reduccion = (double)(afd_sel.estados.size() - afd_min_sel.estados.size())
                               / afd_sel.estados.size() * 100.0;
            std::cout << "Reduccion: " << reduccion << "%\n";
        }
    }

    return 0;
}