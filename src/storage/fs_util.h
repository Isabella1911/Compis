#ifndef SQLINT_STORAGE_FS_UTIL_H
#define SQLINT_STORAGE_FS_UTIL_H

// Utilidades de archivos de la Parte 2. Toda escritura que reemplaza un
// archivo completo (metadata.txt, un CSV reescrito, un indice) pasa por
// writeFileAtomic: se escribe <ruta>.tmp, se cierra y se hace rename. Asi un
// crash a media escritura deja el archivo viejo intacto, nunca uno a medias.

#include <string>

namespace sqlint {
namespace storage {

// "dir" + "/" + "nombre", sin duplicar la barra.
std::string joinPath(const std::string& dir, const std::string& name);

bool fileExists(const std::string& path);
bool dirExists(const std::string& path);

// Crea el directorio y sus padres si no existen. Lanza SqlError RUN103 si
// no se puede.
void ensureDir(const std::string& path);

// Lee el archivo completo, en binario (sin convertir finales de linea).
// Devuelve false si no se pudo abrir.
bool readFile(const std::string& path, std::string& out);

// Reemplaza el archivo de forma atomica (tmp + rename). Lanza SqlError
// RUN103 si falla; en ese caso el archivo original no cambia.
void writeFileAtomic(const std::string& path, const std::string& content);

// Borra el archivo si existe. No es error que no exista.
void removeFile(const std::string& path);

}  // namespace storage
}  // namespace sqlint

#endif  // SQLINT_STORAGE_FS_UTIL_H
