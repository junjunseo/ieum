#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "checker.h"
#include "lexer.h"
#include "parser.h"

namespace {

struct BenchmarkResult {
    std::string scenario;
    std::size_t modules;
    std::size_t layers;
    std::size_t dependencies;
    std::size_t violations;
    std::size_t sourceBytes;
    std::size_t iterations;
    double minMs;
    double medianMs;
    double p95Ms;
    double maxMs;
    std::vector<double> samples;
};

std::size_t parsePositive(const char* value, const char* name) {
    std::string text(value);
    std::size_t consumed = 0;
    unsigned long long parsed = 0;

    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument(std::string(name) + " must be a positive integer");
    }

    try {
        parsed = std::stoull(text, &consumed);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string(name) + " must be a positive integer");
    }

    if (consumed != text.size() || parsed == 0 || parsed > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument(std::string(name) + " must be a positive integer");
    }
    return static_cast<std::size_t>(parsed);
}

std::string moduleName(std::size_t index) {
    return "module_" + std::to_string(index);
}

std::string makeLayeredChain(std::size_t moduleCount) {
    std::ostringstream source;

    for (std::size_t i = 0; i < moduleCount; ++i) {
        source << "module " << moduleName(i);
        if (i + 1 < moduleCount) {
            source << " depends " << moduleName(i + 1);
        }
        source << '\n';
    }

    for (std::size_t i = 0; i + 1 < moduleCount; ++i) {
        source << "layer " << moduleName(i)
               << " above " << moduleName(i + 1) << '\n';
    }

    return source.str();
}

void runPipeline(const std::string& source, std::size_t expectedModules,
                 std::size_t expectedLayers, std::size_t expectedViolations) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    Program program = parser.parse();

    if (program.modules.size() != expectedModules ||
        program.layers.size() != expectedLayers) {
        throw std::runtime_error("benchmark corpus was parsed incorrectly");
    }

    Checker checker(program);
    const auto violations = checker.check();
    if (violations.size() != expectedViolations) {
        throw std::runtime_error("benchmark corpus produced unexpected structural violations");
    }
}

BenchmarkResult benchmark(const std::string& source, std::size_t iterations,
                          std::size_t expectedViolations, const std::string& scenario) {
    Lexer metadataLexer(source);
    Parser metadataParser(metadataLexer.tokenize());
    const Program metadata = metadataParser.parse();
    const auto moduleCount = metadata.modules.size();
    const auto layerCount = metadata.layers.size();
    std::size_t dependencies = 0;
    for (const auto& module : metadata.modules) dependencies += module.deps.size();
    std::vector<double> samples;
    samples.reserve(iterations);

    runPipeline(source, moduleCount, layerCount, expectedViolations);  // Warm-up and correctness check.

    for (std::size_t i = 0; i < iterations; ++i) {
        const auto started = std::chrono::steady_clock::now();
        runPipeline(source, moduleCount, layerCount, expectedViolations);
        const auto finished = std::chrono::steady_clock::now();
        samples.push_back(
            std::chrono::duration<double, std::milli>(finished - started).count());
    }

    const auto chronologicalSamples = samples;
    std::sort(samples.begin(), samples.end());
    const std::size_t medianIndex = samples.size() / 2;
    const double median =
        samples.size() % 2 == 0
            ? (samples[medianIndex - 1] + samples[medianIndex]) / 2.0
            : samples[medianIndex];
    const std::size_t p95Index = (samples.size() * 95 + 99) / 100 - 1;

    return BenchmarkResult{
        scenario,
        moduleCount,
        layerCount,
        dependencies,
        expectedViolations,
        source.size(),
        iterations,
        samples.front(),
        median,
        samples[p95Index],
        samples.back(),
        chronologicalSamples
    };
}

void printResult(const BenchmarkResult& result) {
    const double modulesPerSecond =
        result.medianMs > 0.0
            ? static_cast<double>(result.modules) * 1000.0 / result.medianMs
            : 0.0;

    std::cout << std::fixed << std::setprecision(6)
              << "scenario=" << result.scenario << '\n'
              << "modules=" << result.modules << '\n'
              << "layers=" << result.layers << '\n'
              << "dependencies=" << result.dependencies << '\n'
              << "violations=" << result.violations << '\n'
              << "source_bytes=" << result.sourceBytes << '\n'
              << "iterations=" << result.iterations << '\n'
              << "min_ms=" << result.minMs << '\n'
              << "median_ms=" << result.medianMs << '\n'
              << "p95_ms=" << result.p95Ms << '\n'
              << "max_ms=" << result.maxMs << '\n'
              << "median_modules_per_second=" << modulesPerSecond << '\n';
    std::cout << "warmups=1\nsamples_ms=";
    for (std::size_t i = 0; i < result.samples.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << result.samples[i];
    }
    std::cout << '\n';
#if defined(__VERSION__)
    std::cout << "compiler=" << __VERSION__ << '\n';
#elif defined(_MSC_VER)
    std::cout << "compiler=MSVC " << _MSC_VER << '\n';
#else
    std::cout << "compiler=unknown\n";
#endif
#ifdef NDEBUG
    std::cout << "ndebug=true\n";
#else
    std::cout << "ndebug=false\n";
#endif
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 && !(argc == 5 && std::string(argv[1]) == "--source")) {
        std::cerr << "usage: benchmarkChecker <module-count> <iterations>\n"
                  << "       benchmarkChecker --source <file> <iterations> <expected-violations>\n";
        return 2;
    }

    try {
        if (argc == 5) {
            const std::size_t iterations = parsePositive(argv[3], "iterations");
            if (iterations > 1000) throw std::invalid_argument("iterations must not exceed 1000");
            const std::string expected(argv[4]);
            const std::size_t violations = expected == "0" ? 0 : parsePositive(argv[4], "expected-violations");
            std::ifstream file(argv[2], std::ios::binary);
            if (!file) throw std::runtime_error("cannot open benchmark source");
            std::ostringstream buffer;
            buffer << file.rdbuf();
            if (file.bad()) throw std::runtime_error("cannot read benchmark source");
            printResult(benchmark(buffer.str(), iterations, violations, "corpus-file"));
            return 0;
        }
        const std::size_t moduleCount = parsePositive(argv[1], "module-count");
        const std::size_t iterations = parsePositive(argv[2], "iterations");
        if (moduleCount < 2) {
            throw std::invalid_argument("module-count must be at least 2");
        }

        printResult(benchmark(makeLayeredChain(moduleCount), iterations, 0, "layered-valid-chain"));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "benchmark error: " << error.what() << '\n';
        return 2;
    }
}
