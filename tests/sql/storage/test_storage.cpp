// Pruebas unitarias de la Parte 2 que no necesitan ANTLR: valores, CSV,
// catalogo, TableStore e indices. make test-storage

#include <fstream>
#include <sstream>

#include "check.h"
#include "diagnostics/diagnostic.h"
#include "index/index_file.h"
#include "index/index_manager.h"
#include "storage/catalog.h"
#include "storage/csv.h"
#include "storage/fs_util.h"
#include "storage/table_store.h"
#include "storage/value.h"

using namespace sqlint;
using namespace sqlint::storage;

namespace {

const SqlType kInt{TypeKind::Int, 0};
const SqlType kFloat{TypeKind::Float, 0};
const SqlType kText{TypeKind::Varchar, 50};
const SqlType kBool{TypeKind::Boolean, 0};
const SqlType kDate{TypeKind::Date, 0};

bool same(const Value& a, const Value& b) {
    if (a.isNull() || b.isNull()) return a.isNull() && b.isNull();
    return a.kind == b.kind && compareValues(a, b) == 0;
}

bool roundTrip(const Value& v, const SqlType& t) { return same(fromCsvField(toCsvField(v), t), v); }

std::string slurp(const std::string& path) {
    std::string s;
    readFile(path, s);
    return s;
}

void testValues() {
    // parseDate: fechas reales, bisiestos incluidos.
    Date d;
    CHECK(parseDate("2024-02-29", d) && d.y == 2024 && d.m == 2 && d.d == 29);
    CHECK(!parseDate("2026-02-29", d));
    CHECK(parseDate("2000-02-29", d));
    CHECK(!parseDate("1900-02-29", d));
    CHECK(!parseDate("2026-13-01", d));
    CHECK(!parseDate("2026-04-31", d));
    CHECK(!parseDate("2026-4-01", d));
    CHECK(parseDate("0001-01-01", d) && parseDate("9999-12-31", d));

    // compareValues
    CHECK(compareValues(Value::ofInt(2), Value::ofFloat(2.0)) == 0);
    CHECK(compareValues(Value::ofInt(2), Value::ofFloat(2.5)) < 0);
    CHECK(compareValues(Value::ofString("B"), Value::ofString("a")) < 0);  // byte a byte
    CHECK(compareValues(Value::ofString("abc"), Value::ofString("abd")) < 0);
    CHECK(compareValues(Value::ofBool(false), Value::ofBool(true)) < 0);
    CHECK(compareValues(Value::ofDate({2024, 1, 15}), Value::ofDate({2023, 12, 31})) > 0);
    CHECK(compareValues(Value::ofDate({2024, 2, 1}), Value::ofDate({2024, 1, 31})) > 0);

    // Ida y vuelta de cada tipo.
    CHECK(roundTrip(Value::null(), kInt));
    CHECK(roundTrip(Value::null(), kText));
    CHECK(roundTrip(Value::ofInt(0), kInt));
    CHECK(roundTrip(Value::ofInt(-9223372036854775807LL - 1), kInt));
    CHECK(roundTrip(Value::ofInt(9223372036854775807LL), kInt));
    for (double x : {0.0, -0.5, 0.1, 1.0 / 3.0, 1e300, 5e-324, 123456789.123456789, 8.5}) {
        CHECK(roundTrip(Value::ofFloat(x), kFloat));
    }
    CHECK(toCsvField(Value::ofFloat(0.1)) == "0.1");
    CHECK(toCsvField(Value::ofFloat(20.0)) == "20");
    CHECK(roundTrip(Value::ofBool(true), kBool) && roundTrip(Value::ofBool(false), kBool));
    CHECK(toCsvField(Value::ofBool(true)) == "TRUE");
    CHECK(roundTrip(Value::ofDate({2024, 2, 29}), kDate));
    CHECK(toCsvField(Value::ofDate({5, 1, 2})) == "0005-01-02");
    for (const char* s : {"", "Ana", "Pérez, Luis", "dijo \"hola\"", "O'Neil", "linea1\nlinea2",
                          " espacios ", "con\ttab", "\"", ",", "a\r\nb"}) {
        CHECK(roundTrip(Value::ofString(s), kText));
    }

    // NULL contra cadena vacia.
    CHECK(toCsvField(Value::null()) == "");
    CHECK(toCsvField(Value::ofString("")) == "\"\"");
    CHECK(fromCsvField("", kText).isNull());
    CHECK(fromCsvField("\"\"", kText).asString().empty());
    CHECK(toCsvField(Value::ofString("Pérez, Luis")) == "\"Pérez, Luis\"");
    CHECK(toCsvField(Value::ofString("Ana")) == "Ana");

    // Campos invalidos para su tipo.
    CHECK_THROWS_CODE(fromCsvField("abc", kInt), "RUN103");
    CHECK_THROWS_CODE(fromCsvField("1.5", kInt), "RUN103");
    CHECK_THROWS_CODE(fromCsvField("99999999999999999999", kInt), "RUN103");
    CHECK_THROWS_CODE(fromCsvField("2026-02-30", kDate), "RUN103");
    CHECK_THROWS_CODE(fromCsvField("SI", kBool), "RUN103");
    CHECK_THROWS_CODE(fromCsvField("\"abc", kText), "RUN103");

    SqlType t;
    CHECK(parseTypeName("VARCHAR(50)", t) && t.kind == TypeKind::Varchar && t.length == 50);
    CHECK(parseTypeName("int", t) && t.kind == TypeKind::Int);
    CHECK(!parseTypeName("VARCHAR(0)", t) && !parseTypeName("TEXT", t));
    CHECK(utf8Length("Pérez") == 5);
}

void testCsv() {
    std::string text = "a,b\n1,\"x,y\"\n2,\"multi\nlinea\"\r\n3,\"\"\"q\"\"\"\n4,\n";
    std::istringstream in(text);
    CsvReader reader(in);
    CsvRecord r;
    std::vector<CsvRecord> recs;
    while (reader.next(r)) recs.push_back(r);
    CHECK(recs.size() == 5);
    if (recs.size() == 5) {
        CHECK(recs[1].offset == 4 && recs[1].fields[1] == "\"x,y\"");
        CHECK(recs[2].offset == 12 && recs[2].fields[1] == "\"multi\nlinea\"");
        CHECK(recs[3].line == 5 && recs[3].fields[1] == "\"\"\"q\"\"\"");
        CHECK(text.substr(recs[3].offset, 2) == "3,");
        CHECK(recs[4].fields.size() == 2 && recs[4].fields[1].empty());
    }

    std::istringstream bad("1,\"abierta\n");
    CsvReader badReader(bad);
    bool threw = false;
    try {
        badReader.next(r);
    } catch (const CsvError&) {
        threw = true;
    }
    CHECK(threw);
}

TableDef estudiantes() {
    TableDef t;
    t.name = "estudiantes";
    t.columns = {{"id", kInt, true, true},
                 {"nombre", kText, true, false},
                 {"edad", kInt, false, false},
                 {"ingreso", kDate, false, false}};
    return t;
}

void testCatalog() {
    std::string dir = check::tempDir("catalog");
    Catalog empty = Catalog::load(joinPath(dir, "no_existe"));
    CHECK(empty.tables().empty());
    CHECK(!dirExists(joinPath(dir, "no_existe")));  // load no escribe nada

    Catalog c = Catalog::load(joinPath(dir, "db"));
    c.addTable(estudiantes());
    c.addIndex({"idx_edad", "estudiantes", "edad", "indices/idx_edad.idx"});
    c.save();
    CHECK(fileExists(joinPath(dir, "db/metadata.txt")));
    CHECK(!fileExists(joinPath(dir, "db/metadata.txt.tmp")));

    Catalog back = Catalog::load(joinPath(dir, "db"));
    const TableDef* t = back.findTable("ESTUDIANTES");
    CHECK(t != nullptr);
    if (t != nullptr) {
        CHECK(t->columns.size() == 4 && t->columns[1].type.length == 50);
        CHECK(t->columns[0].primaryKey && t->columns[0].notNull && t->primaryKeyColumn() == 0);
        CHECK(t->findColumn("Edad") == 2);
    }
    CHECK(back.findIndex("IDX_EDAD") != nullptr);
    CHECK(back.indexesOn("estudiantes", "EDAD").size() == 1);
    CHECK(back.toJson().find("\"VARCHAR(50)\"") != std::string::npos);

    // Corrupto: RUN103 con la linea.
    std::string bad = joinPath(dir, "bad");
    ensureDir(bad);
    writeFileAtomic(joinPath(bad, "metadata.txt"), "VERSION 1\nTABLE t\n  COLUMN a TEXTO\nEND\n");
    std::string msg;
    try {
        Catalog::load(bad);
    } catch (const diagnostics::SqlError& e) {
        msg = e.diag.code + " " + e.diag.message;
    }
    CHECK(msg.rfind("RUN103", 0) == 0 && msg.find("línea 3") != std::string::npos);
    writeFileAtomic(joinPath(bad, "metadata.txt"), "TABLE t\n  COLUMN a INT\n");
    CHECK_THROWS_CODE(Catalog::load(bad), "RUN103");
}

void testTableStore() {
    std::string dir = check::tempDir("store");
    TableDef t = estudiantes();
    TableStore store(dir);
    store.create(t);
    CHECK(slurp(store.pathOf(t)) == "id,nombre,edad,ingreso\n");
    CHECK(store.scan(t).empty());

    uint64_t o1 = store.append(t, {Value::ofInt(1), Value::ofString("Ana"), Value::ofInt(20),
                                   Value::ofDate({2024, 1, 15})});
    uint64_t o2 = store.append(t, {Value::ofInt(2), Value::ofString("Pérez, Luis"),
                                   Value::ofInt(17), Value::null()});
    uint64_t o3 = store.append(t, {Value::ofInt(3), Value::ofString(""), Value::null(),
                                   Value::null()});
    CHECK(o1 == 23);
    CHECK(slurp(store.pathOf(t)) ==
          "id,nombre,edad,ingreso\n1,Ana,20,2024-01-15\n2,\"Pérez, Luis\",17,\n3,\"\",,\n");

    std::vector<StoredRow> rows = store.scan(t);
    CHECK(rows.size() == 3);
    if (rows.size() == 3) {
        CHECK(rows[0].offset == o1 && rows[1].offset == o2 && rows[2].offset == o3);
        CHECK(rows[1].values[1].asString() == "Pérez, Luis" && rows[1].values[3].isNull());
        CHECK(!rows[2].values[1].isNull() && rows[2].values[1].asString().empty());
        CHECK(rows[2].values[2].isNull());
    }

    std::vector<StoredRow> some = store.readAt(t, {o3, o1, o3});
    CHECK(some.size() == 2 && some[0].values[0].asInt() == 1 && some[1].values[0].asInt() == 3);
    CHECK_THROWS_CODE(store.readAt(t, {o1 + 1}), "RUN103");
    CHECK_THROWS_CODE(store.readAt(t, {0}), "RUN103");

    // rewrite devuelve los offsets nuevos y readAt los encuentra.
    std::vector<Row> keep = {rows[2].values, rows[0].values};
    std::vector<uint64_t> offs = store.rewrite(t, keep);
    CHECK(offs.size() == 2 && store.readAt(t, {offs[1]})[0].values[0].asInt() == 1);
    CHECK(!fileExists(store.pathOf(t) + ".tmp"));

    // CRLF (editado en Windows) y sin "\n" final.
    writeFileAtomic(store.pathOf(t), "id,nombre,edad,ingreso\r\n1,Ana,20,\r\n2,Luis,,");
    rows = store.scan(t);
    CHECK(rows.size() == 2 && rows[0].values[3].isNull() && rows[1].values[2].isNull());
    uint64_t o4 = store.append(t, {Value::ofInt(4), Value::ofString("Eva"), Value::null(),
                                   Value::null()});
    CHECK(store.readAt(t, {o4})[0].values[1].asString() == "Eva");
    CHECK(store.scan(t).size() == 3);

    // Faltante o corrupto: RUN103.
    writeFileAtomic(store.pathOf(t), "id,nombre,edad,ingreso\n1,Ana,veinte,\n");
    CHECK_THROWS_CODE(store.scan(t), "RUN103");
    writeFileAtomic(store.pathOf(t), "id,nombre\n");
    CHECK_THROWS_CODE(store.scan(t), "RUN103");
    store.drop(t);
    CHECK(!fileExists(store.pathOf(t)));
    CHECK_THROWS_CODE(store.scan(t), "RUN103");
}

void testIndexes() {
    std::string dir = check::tempDir("index");
    Catalog catalog = Catalog::load(dir);
    TableDef t = estudiantes();
    catalog.addTable(t);
    catalog.save();
    TableStore store(dir);
    store.create(t);
    int ages[] = {20, 17, 19, 20, 0};
    for (int i = 0; i < 5; ++i) {
        store.append(t, {Value::ofInt(i + 1), Value::ofString("n" + std::to_string(i)),
                         i == 4 ? Value::null() : Value::ofInt(ages[i]),
                         Value::ofDate({2024, 1, 10 + i})});
    }
    std::vector<StoredRow> rows = store.scan(t);

    index::IndexManager indexes(catalog, store);
    indexes.create("idx_edad", "estudiantes", "edad");
    indexes.create("idx_ingreso", "estudiantes", "ingreso");
    const IndexDef& edad = *catalog.findIndex("idx_edad");
    CHECK(Catalog::load(dir).findIndex("idx_edad") != nullptr);

    // Formato del archivo: un valor por linea, ordenado, sin NULL.
    std::string expected = "# INDEX idx_edad ON estudiantes(edad) TYPE INT\n17\t" +
                           std::to_string(rows[1].offset) + "\n19\t" +
                           std::to_string(rows[2].offset) + "\n20\t" +
                           std::to_string(rows[0].offset) + "," +
                           std::to_string(rows[3].offset) + "\n";
    CHECK(slurp(catalog.indexPath(edad)) == expected);
    index::IndexData data = index::readIndexFile(catalog.indexPath(edad));
    CHECK(data.type.kind == TypeKind::Int && data.entries.size() == 3);

    // Busquedas.
    CHECK(indexes.lookupEq(edad, Value::ofInt(20)).size() == 2);
    CHECK(indexes.lookupEq(edad, Value::ofFloat(20.0)).size() == 2);  // 18.0 encuentra 18
    CHECK(indexes.lookupEq(edad, Value::ofFloat(20.5)).empty());
    CHECK(indexes.lookupEq(edad, Value::null()).empty());
    CHECK(indexes.lookupRange(edad, Value::ofInt(18), true, std::nullopt, false).size() == 3);
    CHECK(indexes.lookupRange(edad, std::nullopt, false, Value::ofInt(19), true).size() == 2);
    CHECK(indexes.lookupRange(edad, std::nullopt, false, Value::ofInt(19), false).size() == 1);
    CHECK(indexes.lookupRange(edad, Value::ofInt(17), false, Value::ofInt(20), false).size() == 1);
    CHECK(indexes.lookupRange(edad, Value::ofInt(20), true, Value::ofInt(17), true).empty());
    CHECK(indexes.lookupRange(edad, Value::ofInt(19), true, Value::ofInt(19), false).empty());
    const IndexDef& ingreso = *catalog.findIndex("idx_ingreso");
    CHECK(indexes.lookupEq(ingreso, Value::ofString("2024-01-12")).size() == 1);
    CHECK(indexes.lookupEq(ingreso, Value::ofString("no es fecha")).empty());

    // Lo que devuelve el indice es lo mismo que filtrar el scan.
    std::vector<StoredRow> viaIndex =
        store.readAt(t, indexes.lookupRange(edad, Value::ofInt(18), true, std::nullopt, false));
    CHECK(viaIndex.size() == 3);
    for (const auto& r : viaIndex) CHECK(r.values[2].asInt() >= 18);

    // Incremental tras append.
    Row nueva = {Value::ofInt(6), Value::ofString("n5"), Value::ofInt(17), Value::null()};
    uint64_t off = store.append(t, nueva);
    indexes.onInsert(t, {StoredRow{off, nueva}});
    CHECK(indexes.lookupEq(edad, Value::ofInt(17)).size() == 2);
    index::IndexManager fresh(catalog, store);  // sin cache: lee el archivo
    CHECK(fresh.lookupEq(edad, Value::ofInt(17)).size() == 2);

    // Reconstruccion tras rewrite.
    std::vector<Row> remaining;
    for (const auto& r : store.scan(t)) {
        if (r.values[0].asInt() % 2 == 0) remaining.push_back(r.values);
    }
    std::vector<uint64_t> offs = store.rewrite(t, remaining);
    std::vector<StoredRow> now;
    for (size_t i = 0; i < offs.size(); ++i) now.push_back({offs[i], remaining[i]});
    indexes.rebuild(t, now);
    std::vector<StoredRow> got = store.readAt(t, indexes.lookupEq(edad, Value::ofInt(17)));
    CHECK(got.size() == 2 && got[0].values[0].asInt() == 2 && got[1].values[0].asInt() == 6);

    // DROP INDEX borra el archivo y la entrada.
    std::string path = catalog.indexPath(ingreso);
    indexes.drop("IDX_INGRESO");
    CHECK(!fileExists(path) && catalog.findIndex("idx_ingreso") == nullptr);
    CHECK(Catalog::load(dir).findIndex("idx_ingreso") == nullptr);

    // Indice corrupto: RUN103.
    writeFileAtomic(path, "# INDEX x ON estudiantes(edad) TYPE INT\nabc\t1\n");
    CHECK_THROWS_CODE(index::readIndexFile(path), "RUN103");
}

}  // namespace

int main() {
    testValues();
    testCsv();
    testCatalog();
    testTableStore();
    testIndexes();
    return check::summary("storage");
}
