// Pruebas del Mutator con scripts SQL de verdad: parser y semantico de la
// Parte 1, Mutator e indices de la Parte 2, y el evaluador provisional
// (fallback_exec) en lugar del de la Parte 3. make test-mutator
//
// Tambien verifica que databases/escuela/crear.sql produce exactamente los
// archivos de databases/escuela. Para regenerarlos despues de cambiar
// crear.sql:
//   build/test_mutator --generar-escuela databases/escuela

#include <filesystem>
#include <fstream>
#include <sstream>

#include "check.h"
#include "diagnostics/reporter.h"
#include "frontend/parser_driver.h"
#include "index/index_manager.h"
#include "semantic/analyzer.h"
#include "storage/catalog.h"
#include "storage/fallback_exec.h"
#include "storage/fs_util.h"
#include "storage/mutator.h"
#include "storage/table_store.h"

using namespace sqlint;
using namespace sqlint::storage;
namespace fs = std::filesystem;

namespace {

struct Outcome {
    std::string error;  // codigo del primer error, "" si todo salio bien
    diagnostics::SourceLoc loc;
    std::vector<MutationResult> results;
};

// Lo que hara la Session de la Parte 3, reducido a las sentencias que
// modifican: sentencia por sentencia, semantico contra el catalogo actual y
// despues el Mutator. Se detiene en el primer error.
Outcome run(const std::string& dir, const std::string& sql) {
    Outcome out;
    diagnostics::DiagnosticReporter reporter;
    frontend::ParseOutput parsed = frontend::parse(sql, reporter);
    if (!parsed.script) {
        out.error = reporter.sorted().front().code;
        return out;
    }
    Catalog catalog;
    try {
        catalog = Catalog::load(dir);
    } catch (const diagnostics::SqlError& e) {
        out.error = e.diag.code;
        return out;
    }
    TableStore store(dir);
    index::IndexManager indexes(catalog, store);
    Mutator mutator(catalog, store, indexes, fallbackHooks(store));
    for (auto& stmt : parsed.script->statements) {
        // CREATE/DROP INDEX son de la entrega 2: hoy el semantico responde
        // SEM023, asi que aqui se ejecutan sin pasar por el.
        bool isIndex = stmt->kind == ast::StatementKind::CreateIndex ||
                       stmt->kind == ast::StatementKind::DropIndex;
        if (!isIndex) {
            std::vector<diagnostics::Diagnostic> diags = semantic::analyze(*stmt, catalog);
            if (!diags.empty()) {
                out.error = diags.front().code;
                out.loc = diags.front().loc;
                return out;
            }
        }
        if (stmt->kind == ast::StatementKind::Select) continue;
        try {
            out.results.push_back(mutator.execute(*stmt));
        } catch (const diagnostics::SqlError& e) {
            out.error = e.diag.code;
            out.loc = e.diag.loc;
            return out;
        }
    }
    return out;
}

std::string slurp(const std::string& path) {
    std::string s;
    readFile(path, s);
    return s;
}

std::vector<StoredRow> scan(const std::string& dir, const std::string& table) {
    Catalog c = Catalog::load(dir);
    TableStore store(dir);
    return store.scan(*c.findTable(table));
}

// Todos los archivos de la base (nombre relativo -> contenido).
std::map<std::string, std::string> snapshot(const std::string& dir) {
    std::map<std::string, std::string> files;
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
        if (e.is_regular_file()) files[fs::relative(e.path(), dir).string()] = slurp(e.path().string());
    }
    return files;
}

const char* kTabla =
    "CREATE TABLE t (id INT PRIMARY KEY, nombre VARCHAR(10) NOT NULL, edad INT, "
    "promedio FLOAT, activo BOOLEAN, ingreso DATE);\n";

// Las rutas databases/... son relativas a la raiz del repo, desde donde
// corre make test-mutator.
void testEscuela() {
    std::string dir = check::tempDir("escuela");
    Outcome o = run(dir, slurp("databases/escuela/crear.sql"));
    CHECK(o.error.empty());
    if (!o.error.empty()) std::printf("        crear.sql: %s en %d:%d\n", o.error.c_str(), o.loc.line, o.loc.col);
    for (const char* f : {"metadata.txt", "estudiantes.csv", "cursos.csv", "inscripciones.csv"}) {
        bool same = slurp(joinPath(dir, f)) == slurp(joinPath("databases/escuela", f));
        CHECK(same);
        if (!same) std::printf("        databases/escuela/%s no coincide con crear.sql\n", f);
    }
    CHECK(scan("databases/escuela", "estudiantes").size() == 25);
    CHECK(scan("databases/escuela", "cursos").size() == 20);
}

void testCreateInsertDrop() {
    std::string dir = check::tempDir("ddl");
    Outcome o = run(dir, std::string(kTabla) +
                             "INSERT INTO t VALUES (1, 'Ana', 20, 8, TRUE, '2024-01-15'),"
                             " (2, 'Luis', NULL, 7.5, FALSE, NULL);\n"
                             "INSERT INTO t (nombre, id) VALUES ('Eva', 3);\n");
    CHECK(o.error.empty() && o.results.size() == 3);
    if (o.results.size() == 3) {
        CHECK(o.results[0].message == "Tabla 't' creada");
        CHECK(o.results[1].affected == 2 && o.results[1].message == "2 filas insertadas");
        CHECK(o.results[2].message == "1 fila insertada");
    }
    CHECK(slurp(joinPath(dir, "t.csv")) ==
          "id,nombre,edad,promedio,activo,ingreso\n"
          "1,Ana,20,8,TRUE,2024-01-15\n2,Luis,,7.5,FALSE,\n3,Eva,,,,\n");
    std::vector<StoredRow> rows = scan(dir, "t");
    CHECK(rows.size() == 3 && rows[0].values[3].kind == TypeKind::Float);  // INT en FLOAT
    CHECK(rows[0].values[5].kind == TypeKind::Date);                       // cadena en DATE

    // DROP y CREATE de nuevo con el mismo nombre: arranca vacia.
    o = run(dir, "DROP TABLE t;\nCREATE TABLE T (x INT);\nINSERT INTO t VALUES (5);\n");
    CHECK(o.error.empty());
    CHECK(slurp(joinPath(dir, "T.csv")) == "x\n5\n" && !fileExists(joinPath(dir, "t.csv")));
    CHECK(Catalog::load(dir).tables().size() == 1);

    // DELETE sin WHERE: vacia la tabla y deja el encabezado.
    o = run(dir, "DELETE FROM t;");
    CHECK(o.error.empty() && o.results[0].affected == 1);
    CHECK(slurp(joinPath(dir, "T.csv")) == "x\n");
}

void testRun101() {
    std::string dir = check::tempDir("run101");
    run(dir, std::string(kTabla) + "INSERT INTO t (id, nombre) VALUES (1, 'Ana'), (2, 'Luis');");
    auto before = snapshot(dir);

    Outcome o = run(dir, "INSERT INTO t (id, nombre) VALUES (3, 'Eva'), (1, 'Otra');");
    CHECK(o.error == "RUN101" && o.loc.line == 1 && o.loc.col == 48);
    CHECK(snapshot(dir) == before);  // no se inserto ninguna de las dos

    o = run(dir, "INSERT INTO t (id, nombre) VALUES (7, 'Eva'), (7, 'Otra');");
    CHECK(o.error == "RUN101");  // repetida dentro del mismo INSERT
    CHECK(snapshot(dir) == before);

    o = run(dir, "UPDATE t SET id = 1 WHERE id = 2;");
    CHECK(o.error == "RUN101");
    CHECK(snapshot(dir) == before);

    o = run(dir, "UPDATE t SET id = id + 10;");  // todas cambian, ninguna choca
    CHECK(o.error.empty() && o.results[0].affected == 2);
    o = run(dir, "INSERT INTO t (id, nombre) VALUES (1, 'Ana');");
    CHECK(o.error.empty());
}

void testRun102() {
    std::string dir = check::tempDir("run102");
    run(dir, "CREATE TABLE p (a INT NOT NULL, b INT);\nINSERT INTO p VALUES (1, NULL), (2, 5);");
    auto before = snapshot(dir);
    Outcome o = run(dir, "UPDATE p SET a = b + 1;");  // la primera fila da NULL
    CHECK(o.error == "RUN102" && o.loc.col == 18);
    CHECK(snapshot(dir) == before);
    o = run(dir, "UPDATE p SET a = b + 1 WHERE b IS NOT NULL;");
    CHECK(o.error.empty() && o.results[0].affected == 1);
    CHECK(slurp(joinPath(dir, "p.csv")) == "a,b\n1,\n6,5\n");
}

void testRun104() {
    std::string dir = check::tempDir("run104");
    run(dir, "CREATE TABLE s (corto VARCHAR(3), largo VARCHAR(20));\n"
             "INSERT INTO s VALUES ('ab', 'abc'), ('xy', 'abcdef');");
    auto before = snapshot(dir);
    Outcome o = run(dir, "UPDATE s SET corto = largo;");
    CHECK(o.error == "RUN104");
    CHECK(snapshot(dir) == before);
    o = run(dir, "UPDATE s SET corto = largo WHERE largo = 'abc';");
    CHECK(o.error.empty());
    CHECK(slurp(joinPath(dir, "s.csv")) == "corto,largo\nabc,abc\nxy,abcdef\n");
}

void testRun103() {
    std::string dir = check::tempDir("run103");
    run(dir, std::string(kTabla) + "INSERT INTO t (id, nombre) VALUES (1, 'Ana');");
    writeFileAtomic(joinPath(dir, "t.csv"), "id,nombre,edad,promedio,activo,ingreso\n1,Ana,x,,,\n");
    CHECK(run(dir, "DELETE FROM t;").error == "RUN103");
    removeFile(joinPath(dir, "t.csv"));
    CHECK(run(dir, "INSERT INTO t (id, nombre) VALUES (2, 'Luis');").error == "RUN103");
    writeFileAtomic(joinPath(dir, "metadata.txt"), "VERSION 1\nTABLE t\n");
    CHECK(run(dir, "DROP TABLE t;").error == "RUN103");
}

void testUpdateSemantics() {
    std::string dir = check::tempDir("swap");
    run(dir, "CREATE TABLE w (a INT, b INT);\nINSERT INTO w VALUES (1, 2), (3, 4), (NULL, 5);");
    Outcome o = run(dir, "UPDATE w SET a = b, b = a WHERE a < 3;");
    CHECK(o.error.empty() && o.results[0].affected == 1 && o.results[0].message == "1 fila actualizada");
    CHECK(slurp(joinPath(dir, "w.csv")) == "a,b\n2,1\n3,4\n,5\n");
    o = run(dir, "DELETE FROM w WHERE a IN (2, 3) OR a IS NULL;");
    CHECK(o.error.empty() && o.results[0].affected == 3);
    o = run(dir, "UPDATE w SET a = 1 WHERE a > 100;");
    CHECK(o.error.empty() && o.results[0].message == "0 filas actualizadas");
}

// La prueba mas importante de los indices: despues de INSERT, UPDATE y
// DELETE, buscar por el indice da lo mismo que filtrar todo el archivo.
bool indexAgrees(const std::string& dir, int lo) {
    Catalog c = Catalog::load(dir);
    TableStore store(dir);
    index::IndexManager indexes(c, store);
    const TableDef& t = *c.findTable("t");
    const IndexDef& idx = *c.findIndex("idx_edad");
    std::vector<StoredRow> viaIndex =
        store.readAt(t, indexes.lookupRange(idx, Value::ofInt(lo), true, std::nullopt, false));
    std::vector<int64_t> a, b;
    for (const auto& r : viaIndex) a.push_back(r.values[0].asInt());
    for (const auto& r : store.scan(t)) {
        if (!r.values[2].isNull() && r.values[2].asInt() >= lo) b.push_back(r.values[0].asInt());
    }
    return a == b;
}

void testIndexMaintenance() {
    std::string dir = check::tempDir("indices");
    Outcome o = run(dir, std::string(kTabla) +
                             "INSERT INTO t (id, nombre, edad) VALUES (1, 'a', 20), (2, 'b', 17),"
                             " (3, 'c', NULL), (4, 'd', 19);\n"
                             "CREATE INDEX idx_edad ON t (edad);\n");
    CHECK(o.error.empty());
    CHECK(fileExists(joinPath(dir, "indices/idx_edad.idx")));
    CHECK(indexAgrees(dir, 18));

    CHECK(run(dir, "INSERT INTO t (id, nombre, edad) VALUES (5, 'e', 18), (6, 'f', 30);").error.empty());
    CHECK(indexAgrees(dir, 18) && indexAgrees(dir, 0));

    CHECK(run(dir, "UPDATE t SET edad = edad + 1 WHERE edad < 19;").error.empty());
    CHECK(indexAgrees(dir, 18) && indexAgrees(dir, 19));

    CHECK(run(dir, "DELETE FROM t WHERE id = 1 OR edad = 30;").error.empty());
    CHECK(indexAgrees(dir, 18) && indexAgrees(dir, 0));

    // Un INSERT que falla tampoco toca el indice.
    auto before = snapshot(dir);
    CHECK(run(dir, "INSERT INTO t (id, nombre, edad) VALUES (8, 'h', 50), (2, 'x', 1);").error ==
          "RUN101");
    CHECK(snapshot(dir) == before);

    CHECK(run(dir, "DROP INDEX idx_edad;").error.empty());
    CHECK(!fileExists(joinPath(dir, "indices/idx_edad.idx")));
    CHECK(run(dir, "CREATE INDEX idx_edad ON t (edad);\nCREATE INDEX idx_nombre ON t (nombre);").error.empty());
    CHECK(run(dir, "DROP TABLE t;").error.empty());
    CHECK(!fileExists(joinPath(dir, "indices/idx_edad.idx")));
    CHECK(!fileExists(joinPath(dir, "indices/idx_nombre.idx")));
    CHECK(Catalog::load(dir).indexes().empty());
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 3 && std::string(argv[1]) == "--generar-escuela") {
        std::string dir = argv[2];
        for (const char* f : {"metadata.txt", "estudiantes.csv", "cursos.csv", "inscripciones.csv"}) {
            removeFile(joinPath(dir, f));
        }
        Outcome o = run(dir, slurp(joinPath(dir, "crear.sql")));
        if (!o.error.empty()) {
            std::printf("crear.sql: %s en %d:%d\n", o.error.c_str(), o.loc.line, o.loc.col);
            return 1;
        }
        std::printf("Base generada en %s\n", dir.c_str());
        return 0;
    }
    testEscuela();
    testCreateInsertDrop();
    testRun101();
    testRun102();
    testRun104();
    testRun103();
    testUpdateSemantics();
    testIndexMaintenance();
    return check::summary("mutator");
}
