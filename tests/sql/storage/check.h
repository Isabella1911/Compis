#ifndef SQLINT_TESTS_STORAGE_CHECK_H
#define SQLINT_TESTS_STORAGE_CHECK_H

// Mini framework de las pruebas unitarias de la Parte 2: CHECK cuenta y
// reporta el fallo con archivo y linea, sin abortar el resto.

#include <cstdio>
#include <filesystem>
#include <string>

namespace check {

inline int failures = 0;
inline int total = 0;

inline void report(bool ok, const char* expr, const char* file, int line) {
    ++total;
    if (!ok) {
        ++failures;
        std::printf("  FALLO %s:%d: %s\n", file, line, expr);
    }
}

// Directorio temporal vacio para una prueba.
inline std::string tempDir(const std::string& name) {
    std::filesystem::path p = std::filesystem::temp_directory_path() / ("sqlint_p2_" + name);
    std::filesystem::remove_all(p);
    std::filesystem::create_directories(p);
    return p.string();
}

inline int summary(const char* name) {
    std::printf("%s: %d/%d ok\n", name, total - failures, total);
    return failures == 0 ? 0 : 1;
}

}  // namespace check

#define CHECK(expr) ::check::report(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

// La expresion lanza SqlError con ese codigo.
#define CHECK_THROWS_CODE(expr, want)                                   \
    do {                                                                \
        std::string got_ = "(no lanzo)";                                \
        try {                                                           \
            (void)(expr);                                               \
        } catch (const sqlint::diagnostics::SqlError& e_) {             \
            got_ = e_.diag.code;                                        \
        }                                                               \
        ::check::report(got_ == (want), #expr " lanza " want, __FILE__, __LINE__); \
        if (got_ != (want)) std::printf("        obtuvo: %s\n", got_.c_str()); \
    } while (0)

#endif  // SQLINT_TESTS_STORAGE_CHECK_H
