#include <iostream>
#include <string>
#include "token.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"

void dump(const Program& prog) {
    std::cout << "── 모듈 (" << prog.modules.size() << ") ──\n";
    for (const auto& m : prog.modules) {
        std::cout << "  module " << m.name;
        if (!m.deps.empty()) {
            std::cout << " depends ";
            for (size_t i = 0; i < m.deps.size(); i++) {
                std::cout << m.deps[i];
                if (i + 1 < m.deps.size()) std::cout << ", ";
            }
        }
        std::cout << "  (line " << m.line << ")\n";
    }
    std::cout << "── 계층 (" << prog.layers.size() << ") ──\n";
    for (const auto& l : prog.layers) {
        std::cout << "  layer " << l.upper << " above " << l.lower
                  << "  (line " << l.line << ")\n";
    }
}

int main() {
    std::string source =
        "# Ieum structure declaration example\n"
        "module domain\n"
        "module ui depends domain\n"
        "module infra depends domain, ui\n"
        "module payment depends domain\n"
        "\n"
        "layer ui above domain\n";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();

        Parser parser(tokens);
        Program prog = parser.parse();
        dump(prog);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}