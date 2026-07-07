#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include "token.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "checker.h"

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

Program parse(const std::string& source) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    return parser.parse();
}

void assertParseThrows(const std::string& source, const std::string& name) {
    try {
        parse(source);
        assertTrue(false, name);
    } catch (const std::runtime_error&) {
        assertTrue(true, name);
    }
}

void assertTestCount(int expected) {
    if (passed != expected) {
        std::cerr << "[FAIL] expected " << expected << " pipeline assertions, got "
                  << passed << "\n";
        failed++;
        assert(passed == expected);
    }
}

} // namespace

int main() {
    const std::string validSource =
        "# comments and blank lines are accepted\n"
        "\n"
        "module domain\n"
        "module service depends domain\n"
        "module ui depends service\n"
        "layer ui above service\n"
        "layer service above domain\n";

    try {
        Program prog = parse(validSource);
        assertTrue(prog.modules.size() == 3, "lexer-parser pipeline parses modules");
        assertTrue(prog.layers.size() == 2, "lexer-parser pipeline parses layers");
        assertTrue(prog.modules[1].deps.size() == 1 &&
                   prog.modules[1].deps[0] == "domain",
                   "pipeline preserves dependency target");
        assertTrue(prog.modules[2].line == 5, "pipeline preserves source line numbers");

        Checker checker(prog);
        assertTrue(checker.check().empty(), "valid structure passes full pipeline");
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] valid pipeline threw: " << e.what() << "\n";
        failed++;
        assert(false);
    }

    try {
        Program prog = parse(
            "\xEF\xBB\xBF"
            "module data\n"
            "module service depends data\n");
        assertTrue(prog.modules.size() == 2 && prog.modules[0].name == "data",
                   "parses UTF-8 BOM input");
        assertTrue(prog.modules[1].deps.size() == 1 &&
                   prog.modules[1].deps[0] == "data",
                   "preserves dependency after UTF-8 BOM");
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] BOM input threw: " << e.what() << "\n";
        failed++;
        assert(false);
    }

    {
        Program prog = parse("module data # inline comment\nmodule service depends data\n");
        assertTrue(prog.modules.size() == 2, "ignores inline comments after declarations");
    }

    {
        Program prog = parse("module\tdata\r\nmodule service depends\tdata\r\n");
        assertTrue(prog.modules.size() == 2 && prog.modules[1].deps[0] == "data",
                   "accepts tabs and CRLF line endings");
    }

    {
        Program prog = parse("module api depends data, infra\nmodule data\nmodule infra\n");
        assertTrue(prog.modules[0].deps.size() == 2 &&
                   prog.modules[0].deps[1] == "infra",
                   "parses comma-separated dependencies with spaces");
    }

    {
        Program prog = parse("module _data1\nmodule service2 depends _data1\n");
        assertTrue(prog.modules[0].name == "_data1" &&
                   prog.modules[1].name == "service2",
                   "accepts ASCII identifiers with underscores and digits");
    }

    {
        Program prog = parse("\n\nmodule data\n\n");
        assertTrue(prog.modules.size() == 1 && prog.modules[0].line == 3,
                   "ignores leading and trailing blank lines");
    }

    assertParseThrows("module ui @ domain\n", "rejects unknown character in source");
    assertParseThrows("module ui depends\n", "rejects incomplete depends declaration");
    assertParseThrows("module 데이터\n", "rejects non-ASCII identifier");
    assertParseThrows("depends data\n", "rejects declaration without module or layer keyword");

    assertTestCount(16);

    std::cout << "\nPipeline tests: " << passed << " passed, "
              << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
