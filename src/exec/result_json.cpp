#include "result_json.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include "common/json.h"

namespace sqlint::exec {
using namespace storage;
namespace {
std::string number(double n, bool console) {
    std::ostringstream out; out.imbue(std::locale::classic());
    if (console) out << std::fixed << std::setprecision(6) << n;
    else out << std::setprecision(17) << n;
    std::string s = out.str();
    if (console && s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s;
}
std::string cell(const Value& v) {
    if (v.kind == TypeKind::Varchar) return v.asString();
    if (v.kind == TypeKind::Float) return number(v.asFloat(), true);
    return displayValue(v);
}
}
std::string valueToJson(const Value& v) {
    switch (v.kind) {
        case TypeKind::Null: return "null";
        case TypeKind::Int: return std::to_string(v.asInt());
        case TypeKind::Float: return std::isfinite(v.asFloat()) ? number(v.asFloat(), false) : "null";
        case TypeKind::Boolean: return v.asBool() ? "true" : "false";
        case TypeKind::Varchar: return json::jsonString(v.asString());
        case TypeKind::Date: return json::jsonString(formatDate(v.asDate()));
    }
    return "null";
}
std::string diagnosticsToJson(const std::vector<diagnostics::Diagnostic>& ds) {
    std::ostringstream out; out << '[';
    for (size_t i = 0; i < ds.size(); ++i) {
        const auto& d = ds[i]; if (i) out << ',';
        out << "{\"phase\":" << json::jsonString(diagnostics::phaseName(d.phase))
            << ",\"code\":" << json::jsonString(d.code) << ",\"line\":" << d.loc.line
            << ",\"col\":" << d.loc.col << ",\"length\":" << d.length
            << ",\"message\":" << json::jsonString(d.message) << '}';
    }
    out << ']'; return out.str();
}
std::string statementsToJson(const std::vector<StatementResult>& statements) {
    std::ostringstream out; out << '[';
    for (size_t n = 0; n < statements.size(); ++n) {
        const auto& s = statements[n]; if (n) out << ',';
        out << "{\"index\":" << s.index << ",\"line\":" << s.line
            << ",\"kind\":" << json::jsonString(s.kind) << ",\"status\":" << json::jsonString(s.status)
            << ",\"columns\":[";
        for (size_t i = 0; i < s.columns.size(); ++i) {
            if (i) out << ',';
            out << "{\"name\":" << json::jsonString(s.columns[i].name)
                << ",\"type\":" << json::jsonString(typeName(s.columns[i].type)) << '}';
        }
        out << "],\"rows\":[";
        for (size_t r = 0; r < s.rows.size(); ++r) {
            if (r) out << ',';
            out << '[';
            for (size_t c = 0; c < s.rows[r].size(); ++c) {
                if (c) out << ',';
                out << valueToJson(s.rows[r][c]);
            }
            out << ']';
        }
        out << "],\"affected\":" << (s.affected ? std::to_string(*s.affected) : "null")
            << ",\"plan\":" << json::jsonString(s.plan)
            << ",\"message\":" << json::jsonString(s.message) << '}';
    }
    out << ']'; return out.str();
}
std::string toJson(const RunOutput& output) {
    return "{\"diagnostics\":" + diagnosticsToJson(output.diagnostics) +
        ",\"statements\":" + statementsToJson(output.statements) + "}";
}
std::string formatTable(const StatementResult& s) {
    if (s.columns.empty()) return s.message + "\n";
    std::vector<std::vector<std::string>> rows;
    std::vector<size_t> widths;
    for (const auto& c : s.columns) widths.push_back(utf8Length(c.name));
    for (const auto& row : s.rows) {
        std::vector<std::string> cells;
        for (size_t c = 0; c < row.size(); ++c) {
            cells.push_back(cell(row[c])); widths[c] = std::max(widths[c], size_t(utf8Length(cells.back())));
        }
        rows.push_back(std::move(cells));
    }
    std::ostringstream out;
    auto border = [&] { out << '+'; for (auto w : widths) out << std::string(w + 2, '-') << '+'; out << '\n'; };
    auto print = [&](const std::vector<std::string>& cells) {
        out << '|';
        for (size_t c = 0; c < cells.size(); ++c)
            out << ' ' << cells[c] << std::string(widths[c] - utf8Length(cells[c]) + 1, ' ') << '|';
        out << '\n';
    };
    border(); std::vector<std::string> names;
    for (const auto& c : s.columns) names.push_back(c.name);
    print(names); border(); for (const auto& row : rows) print(row); border();
    out << s.message << '\n'; if (!s.plan.empty()) out << "Plan: " << s.plan << '\n';
    return out.str();
}
}  // namespace sqlint::exec
