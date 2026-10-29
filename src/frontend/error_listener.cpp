#include "error_listener.h"

#include "Token.h"
#include "diagnostics/codes.h"

namespace sqlint {
namespace frontend {

namespace {

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size()) {
        s.replace(pos, from.size(), to);
    }
}

// ANTLR lista lo esperado como "{'FROM', ','}"; un solo elemento va sin llaves.
std::string describeExpected(const std::string& expected) {
    if (expected.size() >= 2 && expected.front() == '{' && expected.back() == '}') {
        return "uno de: " + expected.substr(1, expected.size() - 2);
    }
    return expected;
}

}  // namespace

std::string translateAntlrMessage(const std::string& original) {
    std::string msg = original;
    replaceAll(msg, "'<EOF>'", "el fin del script");
    replaceAll(msg, "<EOF>", "el fin del script");

    // "mismatched input 'X' expecting Y"
    const std::string mismatched = "mismatched input ";
    // "extraneous input 'X' expecting Y"
    const std::string extraneous = "extraneous input ";
    // "missing X at 'Y'"
    const std::string missing = "missing ";
    // "no viable alternative at input 'X'"
    const std::string noViable = "no viable alternative at input ";
    const std::string expecting = " expecting ";

    if (startsWith(msg, mismatched) || startsWith(msg, extraneous)) {
        bool isMismatched = startsWith(msg, mismatched);
        std::string rest = msg.substr(isMismatched ? mismatched.size() : extraneous.size());
        size_t exp = rest.find(expecting);
        std::string found = rest.substr(0, exp);
        std::string head = isMismatched ? "Se encontró " + found : "Sobra " + found;
        if (exp == std::string::npos) return head;
        return head + ", se esperaba " + describeExpected(rest.substr(exp + expecting.size()));
    }
    if (startsWith(msg, missing)) {
        std::string rest = msg.substr(missing.size());
        size_t at = rest.rfind(" at ");
        if (at != std::string::npos) {
            return "Falta " + rest.substr(0, at) + " antes de " + rest.substr(at + 4);
        }
        return "Falta " + rest;
    }
    if (startsWith(msg, noViable)) {
        return "Sentencia inválida cerca de " + msg.substr(noViable.size());
    }
    if (startsWith(msg, "token recognition error at: ")) {
        return "Carácter no reconocido " + msg.substr(28);
    }
    return msg;
}

void DiagnosticErrorListener::syntaxError(antlr4::Recognizer* /*recognizer*/,
                                          antlr4::Token* offendingSymbol, size_t line,
                                          size_t charPositionInLine, const std::string& msg,
                                          std::exception_ptr /*e*/) {
    int length = 1;
    if (offendingSymbol != nullptr && offendingSymbol->getType() != antlr4::Token::EOF) {
        length = static_cast<int>(offendingSymbol->getText().size());
        if (length < 1) length = 1;
    }
    const char* code = phase_ == diagnostics::Phase::Lexico ? diagnostics::codes::LEX001
                                                            : diagnostics::codes::SYN001;
    reporter_.error(phase_, code, translateAntlrMessage(msg),
                    {static_cast<int>(line), static_cast<int>(charPositionInLine) + 1}, length);
}

}  // namespace frontend
}  // namespace sqlint
