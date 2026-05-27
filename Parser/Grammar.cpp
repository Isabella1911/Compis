#include "Grammar.h"
#include <cctype>

bool esToken(const std::string& simbolo) {
    if (simbolo.empty() || simbolo == "$" || simbolo == "epsilon") {
        return false;
    }
    return std::isupper(static_cast<unsigned char>(simbolo[0]));
}

bool esNoTerminal(const std::string& simbolo) {
    if (simbolo.empty() || simbolo == "$" || simbolo == "epsilon") {
        return false;
    }
    return std::islower(static_cast<unsigned char>(simbolo[0]));
}

void validarGramatica(const Gramatica& g) {
    if (g.producciones.empty()) {
        throw std::runtime_error("La gramática no contiene producciones");
    }

    if (g.simboloInicial.empty()) {
        throw std::runtime_error("No se definió símbolo inicial");
    }

    for (const auto& prod : g.producciones) {
        if (prod.izquierda.empty()) {
            throw std::runtime_error("Producción con lado izquierdo vacío");
        }

        if (!esNoTerminal(prod.izquierda)) {
            throw std::runtime_error("Error en YAPar: el lado izquierdo de la producción '"
                + prod.izquierda + "' debe ser un no-terminal (comenzar con minúscula)");
        }

        for (const auto& simbolo : prod.derecha) {
            if (simbolo == "epsilon") {
                continue;
            }

            if (simbolo == "$") {
                throw std::runtime_error("Error en YAPar: el símbolo $ no puede usarse en producciones");
            }

            bool esTerminal = esToken(simbolo);
            bool esNoTerm = esNoTerminal(simbolo);

            if (!esTerminal && !esNoTerm) {
                throw std::runtime_error("Error en YAPar: el símbolo '" + simbolo
                    + "' usado en la producción '" + prod.izquierda
                    + "' no es válido (debe ser MAYÚSCULA o minúscula)");
            }

            if (esTerminal && g.terminales.count(simbolo) == 0) {
                throw std::runtime_error("Error en YAPar: el símbolo " + simbolo
                    + " aparece en la producción '" + prod.izquierda
                    + "' pero no fue declarado con %token");
            }

            if (esNoTerm) {
                bool definido = false;
                for (const auto& p : g.producciones) {
                    if (p.izquierda == simbolo) {
                        definido = true;
                        break;
                    }
                }
                if (!definido) {
                    throw std::runtime_error("Error en YAPar: el no-terminal '" + simbolo
                        + "' usado en la producción '" + prod.izquierda
                        + "' no fue definido");
                }
            }
        }
    }
}
