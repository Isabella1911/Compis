#include "csv.h"

namespace sqlint {
namespace storage {

CsvReader::CsvReader(std::istream& in, char sep, uint64_t offset, int line)
    : in_(in), sep_(sep), offset_(offset), line_(line) {}

int CsvReader::get() {
    int c = in_.get();
    if (c == std::char_traits<char>::eof()) return c;
    ++offset_;
    if (c == '\n') ++line_;
    return c;
}

int CsvReader::peek() { return in_.peek(); }

bool CsvReader::next(CsvRecord& record) {
    const int eof = std::char_traits<char>::eof();
    if (peek() == eof) return false;

    record.offset = offset_;
    record.line = line_;
    record.fields.clear();

    std::string field;
    while (true) {
        int c = get();
        if (c == '"' && field.empty()) {
            // Campo entre comillas: se guarda crudo hasta la comilla final.
            field += '"';
            while (true) {
                c = get();
                if (c == eof) throw CsvError(record.line, "comilla sin cerrar");
                field += static_cast<char>(c);
                if (c == '"') {
                    if (peek() == '"') {
                        field += static_cast<char>(get());
                    } else {
                        break;
                    }
                }
            }
            c = get();
            if (c == '\r' && peek() == '\n') c = get();
            if (c != sep_ && c != '\n' && c != eof) {
                throw CsvError(line_, "texto despues de la comilla final de un campo");
            }
        } else {
            while (c != sep_ && c != '\n' && c != eof) {
                field += static_cast<char>(c);
                c = get();
            }
            if (c != sep_ && !field.empty() && field.back() == '\r') field.pop_back();
        }
        record.fields.push_back(field);
        field.clear();
        if (c != sep_) return true;  // fin de linea o de archivo
    }
}

std::string formatCsvRecord(const std::vector<std::string>& fields, char sep) {
    std::string out;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) out += sep;
        out += fields[i];
    }
    out += '\n';
    return out;
}

}  // namespace storage
}  // namespace sqlint
