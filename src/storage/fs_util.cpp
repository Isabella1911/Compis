#include "fs_util.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "diagnostics/codes.h"
#include "diagnostics/diagnostic.h"

namespace sqlint {
namespace storage {

namespace fs = std::filesystem;

namespace {

[[noreturn]] void ioError(const std::string& message) {
    diagnostics::Diagnostic d;
    d.phase = diagnostics::Phase::Ejecucion;
    d.code = diagnostics::codes::RUN103;
    d.message = message;
    throw diagnostics::SqlError(d);
}

}  // namespace

std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    if (dir.back() == '/') return dir + name;
    return dir + "/" + name;
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

bool dirExists(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

void ensureDir(const std::string& path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) return;
    fs::create_directories(path, ec);
    if (ec) ioError("No se pudo crear el directorio '" + path + "': " + ec.message());
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

void writeFileAtomic(const std::string& path, const std::string& content) {
    std::string tmp = path + ".tmp";
    {
        // El ofstream se cierra antes del rename: en WSL sobre /mnt/c (NTFS)
        // renombrar un archivo abierto puede fallar.
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) ioError("No se pudo escribir '" + tmp + "'");
        out << content;
        out.flush();
        if (!out) ioError("No se pudo escribir '" + tmp + "'");
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        ioError("No se pudo reemplazar '" + path + "'");
    }
}

void removeFile(const std::string& path) {
    std::error_code ec;
    fs::remove(path, ec);
}

}  // namespace storage
}  // namespace sqlint
