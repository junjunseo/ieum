#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "token.h"
#include "ast.h"
#include "parser.h"

namespace {

int passed = 0;
int failed = 0;

void assertTrue(bool condition, const std::string& name) {
    if (condition) {
        std::cout << "[PASS] " << name << "\n";
        passed++;
        return;
    }

    std::cerr << "[FAIL] " << name << "\n";
    failed++;
    assert(condition);
}

Program parseTokens(const std::vector<Token>& tokens) {
    Parser parser(tokens);
    return parser.parse();
}

void assertThrows(const std::vector<Token>& tokens, const std::string& name) {
    try {
        parseTokens(tokens);
        assertTrue(false, name);
    } catch (const std::runtime_error&) {
        assertTrue(true, name);
    }
}

void assertTestCount(int expected) {
    if (passed != expected) {
        std::cerr << "[FAIL] expected " << expected << " parser assertions, got "
                  << passed << "\n";
        failed++;
        assert(passed == expected);
    }
}

} // namespace

int main() {
    const std::vector<Token> validTokens = {
        {TokenType::MODULE,     "module",  1}, {TokenType::IDENTIFIER, "domain", 1}, {TokenType::NEWLINE, "", 1},
        {TokenType::MODULE,     "module",  2}, {TokenType::IDENTIFIER, "ui",     2}, {TokenType::DEPENDS, "depends", 2}, {TokenType::IDENTIFIER, "domain", 2}, {TokenType::NEWLINE, "", 2},
        {TokenType::MODULE,     "module",  3}, {TokenType::IDENTIFIER, "infra",  3}, {TokenType::DEPENDS, "depends", 3}, {TokenType::IDENTIFIER, "domain", 3}, {TokenType::COMMA, ",", 3}, {TokenType::IDENTIFIER, "ui", 3}, {TokenType::NEWLINE, "", 3},
        {TokenType::LAYER,      "layer",   4}, {TokenType::IDENTIFIER, "ui",     4}, {TokenType::ABOVE, "above", 4}, {TokenType::IDENTIFIER, "domain", 4}, {TokenType::NEWLINE, "", 4},
        {TokenType::END,        "",        5},
    };

    try {
        Program prog = parseTokens(validTokens);
        assertTrue(prog.modules.size() == 3, "parses three module declarations");
        assertTrue(prog.modules[0].name == "domain", "preserves first module name");
        assertTrue(prog.modules[1].deps.size() == 1, "parses single dependency");
        assertTrue(prog.modules[1].deps[0] == "domain", "preserves single dependency target");
        assertTrue(prog.modules[2].deps.size() == 2, "parses multiple dependency targets");
        assertTrue(prog.modules[2].deps[1] == "ui", "preserves dependency target order");
        assertTrue(prog.modules[2].line == 3, "preserves module declaration line");
        assertTrue(prog.layers.size() == 1, "parses layer declaration");
        assertTrue(prog.layers[0].upper == "ui" && prog.layers[0].lower == "domain",
                   "preserves layer relation");
        assertTrue(prog.layers[0].line == 4, "preserves layer declaration line");
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] valid token parsing threw: " << e.what() << "\n";
        failed++;
        assert(false);
    }

    {
        const Program prog = parseTokens({
            {TokenType::NEWLINE, "", 1},
            {TokenType::NEWLINE, "", 2},
            {TokenType::END, "", 3},
        });
        assertTrue(prog.modules.empty() && prog.layers.empty(), "skips empty lines");
    }

    {
        const Program prog = parseTokens({
            {TokenType::MODULE, "module", 1},
            {TokenType::IDENTIFIER, "solo", 1},
            {TokenType::END, "", 1},
        });
        assertTrue(prog.modules.size() == 1 && prog.modules[0].name == "solo",
                   "parses final declaration without trailing newline");
    }

    assertThrows({
        {TokenType::MODULE, "module", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "rejects module declaration without name");

    assertThrows({
        {TokenType::MODULE, "module", 1},
        {TokenType::IDENTIFIER, "ui", 1},
        {TokenType::DEPENDS, "depends", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "rejects depends without target");

    assertThrows({
        {TokenType::MODULE, "module", 1},
        {TokenType::IDENTIFIER, "ui", 1},
        {TokenType::DEPENDS, "depends", 1},
        {TokenType::IDENTIFIER, "domain", 1},
        {TokenType::COMMA, ",", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "rejects comma without following dependency");

    assertThrows({
        {TokenType::LAYER, "layer", 1},
        {TokenType::IDENTIFIER, "ui", 1},
        {TokenType::IDENTIFIER, "domain", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "rejects layer declaration without above");

    assertThrows({
        {TokenType::LAYER, "layer", 1},
        {TokenType::IDENTIFIER, "ui", 1},
        {TokenType::ABOVE, "above", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "rejects layer declaration without lower target");

    assertThrows({
        {TokenType::MODULE, "module", 1},
        {TokenType::IDENTIFIER, "ui", 1},
        {TokenType::DEPENDS, "depends", 1},
        {TokenType::IDENTIFIER, "domain", 1},
        {TokenType::MODULE, "module", 1},
        {TokenType::IDENTIFIER, "extra", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "rejects multiple declarations on one line");

    assertThrows({
        {TokenType::IDENTIFIER, "ui", 1},
        {TokenType::NEWLINE, "", 1},
        {TokenType::END, "", 2},
    }, "rejects declarations that do not start with keyword");

    assertThrows({
        {TokenType::UNKNOWN, "@", 1},
        {TokenType::END, "", 1},
    }, "rejects unknown token");

    assertTestCount(20);

    std::cout << "\nParser tests: " << passed << " passed, "
              << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
