#ifndef SQLINT_STORAGE_CSV_H
#define SQLINT_STORAGE_CSV_H

// Lector y escritor CSV estilo RFC 4180 (docs/sql/parte2, seccion 4.2).
//
// El lector no interpreta los campos: los devuelve "crudos", tal como estan
// en el archivo y con sus comillas, porque la diferencia entre un campo
// vacio (NULL) y "" (cadena vacia) la resuelve fromCsvField segun el tipo de
// la columna. Ademas informa el offset en bytes donde empieza cada registro:
// es lo que guardan los indices para que TableStore::readAt haga seekg
// directo a la fila.
//
// Acepta comas, comillas y saltos de linea dentro de un campo entre
// comillas, y tolera "\r\n" (un CSV editado en Windows). El escritor siempre
// usa "\n".
//
// El separador es configurable porque los archivos de indice usan el mismo
// formato con tabulador.

#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>

namespace sqlint {
namespace storage {

struct CsvRecord {
    uint64_t offset = 0;               // byte donde empieza el registro
    int line = 0;                      // linea (1-based) donde empieza
    std::vector<std::string> fields;   // crudos, con comillas
};

// Error de formato (comilla sin cerrar, texto despues de la comilla final).
// TableStore lo convierte en RUN103 con el nombre del archivo.
class CsvError : public std::runtime_error {
public:
    CsvError(int line, const std::string& message)
        : std::runtime_error(message), line(line) {}
    int line;
};

class CsvReader {
public:
    // Lee desde la posicion actual de `in`, que corresponde al byte `offset`
    // del archivo y a la linea `line`.
    explicit CsvReader(std::istream& in, char sep = ',', uint64_t offset = 0, int line = 1);

    // Lee el siguiente registro. Devuelve false al llegar al final.
    bool next(CsvRecord& record);

private:
    int get();   // siguiente byte o EOF, avanzando offset_ y line_
    int peek();

    std::istream& in_;
    char sep_;
    uint64_t offset_;
    int line_;
};

// Un registro listo para escribir: los campos (ya serializados con
// toCsvField) separados por `sep`, terminado en "\n".
std::string formatCsvRecord(const std::vector<std::string>& fields, char sep = ',');

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_CSV_H
