#include "index_file.h"

#include <sstream>

#include "diagnostics/codes.h"
#include "diagnostics/diagnostic.h"
#include "storage/csv.h"
#include "storage/fs_util.h"

namespace sqlint {
namespace index {

using storage::CsvError;
using storage::CsvReader;
using storage::CsvRecord;

namespace {

[[noreturn]] void corrupt(const std::string& path, int line, const std::string& what) {
    diagnostics::Diagnostic d;
    d.phase = diagnostics::Phase::Ejecucion;
    d.code = diagnostics::codes::RUN103;
    d.message = "El índice '" + path + "'" +
                (line > 0 ? " está corrupto en la línea " + std::to_string(line) : "") + ": " +
                what;
    throw diagnostics::SqlError(d);
}

bool parseOffsets(const std::string& text, std::vector<uint64_t>& out) {
    if (text.empty()) return false;
    std::string part;
    std::istringstream in(text);
    while (std::getline(in, part, ',')) {
        if (part.empty() || part.find_first_not_of("0123456789") != std::string::npos) {
            return false;
        }
        out.push_back(std::stoull(part));
    }
    return !out.empty();
}

}  // namespace

void writeIndexFile(const std::string& path, const storage::IndexDef& def,
                    const IndexData& data) {
    std::string content = "# INDEX " + def.name + " ON " + def.table + "(" + def.column +
                          ") TYPE " + storage::typeName(data.type) + "\n";
    for (const auto& [value, offsets] : data.entries) {
        std::string list;
        for (size_t i = 0; i < offsets.size(); ++i) {
            if (i > 0) list += ',';
            list += std::to_string(offsets[i]);
        }
        content += storage::formatCsvRecord({storage::toCsvField(value), list}, '\t');
    }
    size_t slash = path.find_last_of('/');
    if (slash != std::string::npos) storage::ensureDir(path.substr(0, slash));
    storage::writeFileAtomic(path, content);
}

IndexData readIndexFile(const std::string& path) {
    std::string content;
    if (!storage::readFile(path, content)) corrupt(path, 0, "no existe o no se puede abrir");

    // Encabezado: "# INDEX <n> ON <t>(<c>) TYPE <tipo>". Solo importa el tipo.
    IndexData data;
    size_t eol = content.find('\n');
    std::string header = content.substr(0, eol);
    if (!header.empty() && header.back() == '\r') header.pop_back();
    size_t typePos = header.rfind(" TYPE ");
    if (header.rfind("# INDEX ", 0) != 0 || typePos == std::string::npos ||
        !storage::parseTypeName(header.substr(typePos + 6), data.type)) {
        corrupt(path, 1, "encabezado inválido");
    }
    if (eol == std::string::npos) return data;

    std::istringstream in(content.substr(eol + 1));
    CsvReader reader(in, '\t', eol + 1, 2);
    CsvRecord rec;
    try {
        while (reader.next(rec)) {
            if (rec.fields.size() == 1 && rec.fields[0].empty()) continue;  // linea en blanco
            if (rec.fields.size() != 2) corrupt(path, rec.line, "se esperaba valor<TAB>offsets");
            storage::Value value;
            try {
                value = storage::fromCsvField(rec.fields[0], data.type);
            } catch (const diagnostics::SqlError& e) {
                corrupt(path, rec.line, e.diag.message);
            }
            if (value.isNull()) corrupt(path, rec.line, "los NULL no se indexan");
            std::vector<uint64_t> offsets;
            if (!parseOffsets(rec.fields[1], offsets)) corrupt(path, rec.line, "offsets inválidos");
            auto& slot = data.entries[value];
            slot.insert(slot.end(), offsets.begin(), offsets.end());
        }
    } catch (const CsvError& e) {
        corrupt(path, e.line, e.what());
    }
    return data;
}

}  // namespace index
}  // namespace sqlint
