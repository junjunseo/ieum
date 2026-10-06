// In-process execution only: parsing and all static checks precede the timer.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "checker.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"

static std::size_t positive(const std::string& text, std::size_t maximum) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("expected a positive integer");
    const auto n = std::stoull(text);
    if (n == 0 || n > maximum) throw std::invalid_argument("argument outside supported range");
    return static_cast<std::size_t>(n);
}

int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument(
            "usage: benchmarkRuntime <loop|recursion|collections> <size 1..10000 (recursion <=512)> <iterations 1..1000>");
        const std::string scenario = argv[1];
        const auto size = positive(argv[2], scenario == "recursion" ? 512 : 10000);
        const auto iterations = positive(argv[3], 1000);
        const auto n = std::to_string(size);
        std::string source;
        std::int64_t expected = 0;
        if (scenario == "loop") {
            source = "module app {\nfn main() -> int {\nlet sum = 0\n"
                "for (let i = 1; i <= " + n + "; i = i + 1) {\nsum = sum + i\n}\nreturn sum\n}\n}\n";
            expected = static_cast<std::int64_t>(size * (size + 1) / 2);
        } else if (scenario == "recursion") {
            source = "module app {\nfn count(n: int) -> int {\nif (n == 0) {\nreturn 0\n}\n"
                "return 1 + count(n - 1)\n}\nfn main() -> int {\nreturn count(" + n + ")\n}\n}\n";
            expected = static_cast<std::int64_t>(size);
        } else if (scenario == "collections") {
            source = "module app depends std_list {\nrecord Box {\nvalues: list<int>\n}\n"
                "fn main() -> int {\nlet original = Box([10,20,30])\nlet sum = 0\n"
                "for (let i = 0; i < " + n + "; i = i + 1) {\n"
                "let copy = original\ncopy.values[0] = 99\n"
                "for (let j = 0; j < length(original.values); j = j + 1) {\n"
                "sum = sum + original.values[j]\n}\n}\nreturn sum\n}\n}\n";
            expected = static_cast<std::int64_t>(60 * size);
        } else {
            throw std::invalid_argument("unknown scenario");
        }
        auto program = Parser(Lexer(source, "runtime-benchmark.ieum").tokenize()).parse();
        if (!Checker(program).check().empty()) throw std::runtime_error("structural validation failed");
        auto semantics = SemanticAnalyzer(program).analyze();
        if (!semantics.ok()) throw std::runtime_error(semantics.violations.front().message);
        const Interpreter runtime(program, semantics, {5000000, 1024});
        auto verify = [&](const ExecutionResult& result) {
            if (!result.success || !std::holds_alternative<std::int64_t>(result.returnValue)
                || std::get<std::int64_t>(result.returnValue) != expected)
                throw std::runtime_error("runtime result mismatch: " + result.error);
        };
        const auto warmup = runtime.run("app", "main");
        verify(warmup);
        std::vector<double> samples;
        for (std::size_t i = 0; i < iterations; ++i) {
            const auto begin = std::chrono::steady_clock::now();
            auto result = runtime.run("app", "main");
            const auto end = std::chrono::steady_clock::now();
            verify(result);
            if (result.stepsExecuted != warmup.stepsExecuted)
                throw std::runtime_error("non-deterministic execution steps");
            samples.push_back(std::chrono::duration<double, std::milli>(end - begin).count());
        }
        auto ordered = samples;
        std::sort(ordered.begin(), ordered.end());
        const auto middle = iterations / 2;
        const double median = iterations % 2 ? ordered[middle] : (ordered[middle - 1] + ordered[middle]) / 2;
        std::cout << std::fixed << std::setprecision(6)
            << "scenario=" << scenario << "\nsize=" << size
            << "\niterations=" << iterations << "\nwarmups=1\nexpected=" << expected
            << "\nsteps=" << warmup.stepsExecuted
            << "\nmin_ms=" << ordered.front() << "\nmedian_ms=" << median
            << "\np95_ms=" << ordered[static_cast<std::size_t>(std::ceil(iterations * 0.95)) - 1]
            << "\nmax_ms=" << ordered.back() << "\nsamples_ms=";
        for (std::size_t i = 0; i < samples.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << samples[i];
        }
#if defined(__VERSION__)
        std::cout << "\ncompiler=" << __VERSION__;
#elif defined(_MSC_VER)
        std::cout << "\ncompiler=MSVC " << _MSC_VER;
#else
        std::cout << "\ncompiler=unknown";
#endif
#ifdef NDEBUG
        std::cout << "\nndebug=true\n";
#else
        std::cout << "\nndebug=false\n";
#endif
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "benchmark error: " << error.what() << '\n';
        return 2;
    }
}
