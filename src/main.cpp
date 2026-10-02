#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <charconv>
#include <filesystem>
#include "cli_args.h"
#include "token.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "checker.h"
#include "graph.h"
#include "semantic.h"
#include "interpreter.h"
#include "version.h"

// 위반 종류 → 사람이 읽는 라벨
static std::string structuralKindLabel(Violation::Kind k) {
    switch (k) {
        case Violation::Kind::DuplicateModule:      return "중복 모듈";
        case Violation::Kind::UndefinedLayerModule: return "미선언 계층 모듈";
        case Violation::Kind::SelfLayer:            return "자기 계층";
        case Violation::Kind::ImplicitDependency: return "암묵적 의존";
        case Violation::Kind::CyclicDependency:   return "순환 의존";
        case Violation::Kind::LayerViolation:     return "계층 위반";
    }
    return "알 수 없음";
}

static std::string semanticKindLabel(SemanticViolationKind kind) {
    switch (kind) {
        case SemanticViolationKind::DuplicateModuleVariable: return "중복 모듈 변수";
        case SemanticViolationKind::DuplicateFunction:       return "중복 함수";
        case SemanticViolationKind::DuplicateParameter:      return "중복 매개변수";
        case SemanticViolationKind::DuplicateLocalVariable:  return "중복 지역 변수";
        case SemanticViolationKind::UndefinedVariable:       return "미정의 변수";
        case SemanticViolationKind::UndefinedFunction:       return "미정의 함수";
        case SemanticViolationKind::AmbiguousFunction:       return "모호한 함수";
        case SemanticViolationKind::MissingCallDependency:   return "호출 의존 누락";
        case SemanticViolationKind::ArityMismatch:            return "인자 개수 불일치";
        case SemanticViolationKind::TypeMismatch:             return "타입 불일치";
        case SemanticViolationKind::MissingReturn:            return "반환 누락";
        case SemanticViolationKind::InvalidControlFlow:       return "잘못된 제어 흐름";
        case SemanticViolationKind::InvalidType:             return "자료구조 타입 오류";
        case SemanticViolationKind::InvalidField:            return "필드 오류";
        case SemanticViolationKind::IncompleteSignature:     return "불완전한 함수 타입";
    }
    return "알 수 없음";
}

struct BodySummary {
    std::size_t modules = 0;
    std::size_t moduleVariables = 0;
    std::size_t functions = 0;
    std::size_t functionStatements = 0;
};

static std::size_t countStatements(const std::vector<Statement>& body) {
    std::size_t count = body.size();
    for (const auto& statement : body) {
        count += countStatements(statement.body) + countStatements(statement.alternative);
        count += countStatements(statement.initializer) + countStatements(statement.update);
    }
    return count;
}

static BodySummary summarizeBodies(const Program& program) {
    BodySummary summary;
    for (const auto& module : program.modules) {
        if (module.hasBody) summary.modules++;
        summary.moduleVariables += module.variables.size();
        summary.functions += module.functions.size();
        for (const auto& function : module.functions) {
            summary.functionStatements += countStatements(function.body);
        }
    }
    return summary;
}

static bool parseEntryPoint(
    const std::string& text,
    std::string& module,
    std::string& function) {
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= text.size()) {
        return false;
    }
    if (text.find('.', dot + 1) != std::string::npos) return false;

    module = text.substr(0, dot);
    function = text.substr(dot + 1);
    return true;
}

struct CliOptions {
    std::string sourcePath;
    bool shouldRun = false;
    std::string entryModule;
    std::string entryFunction;
    bool shouldEmitDot = false;
    std::string dotPath;
    ExecutionLimits limits;
};

static void printUsage() {
    std::cerr << "사용법: ieum <소스파일.ieum>\n"
              << "       ieum <소스파일.ieum> --run <모듈>.<함수>\n"
              << "       ieum <소스파일.ieum> --emit-dot <출력파일.dot>\n"
              << "       ieum <소스파일.ieum> --run <모듈>.<함수> --emit-dot <출력파일.dot>\n"
              << "       실행 한도: --max-steps <양의 정수> --max-call-depth <양의 정수>\n"
              << "       ieum --version\n";
}

static bool parseLimit(const std::string& text, std::size_t& value) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value > 0;
}

static bool parseCliOptions(int argc, char** argv, CliOptions& options) {
    if (argc < 2) return false;
    options.sourcePath = argv[1];
    bool hasSteps = false, hasDepth = false;

    for (int i = 2; i < argc;) {
        const std::string option = argv[i];
        if (option == "--run") {
            if (options.shouldRun || i + 1 >= argc) return false;
            options.shouldRun = true;
            if (!parseEntryPoint(
                    argv[i + 1], options.entryModule, options.entryFunction)) {
                std::cerr << "오류: 실행 진입점은 <모듈>.<함수> 형식이어야 합니다 "
                          << "(entry_format=module.function)\n";
                return false;
            }
            i += 2;
            continue;
        }
        if (option == "--emit-dot") {
            if (options.shouldEmitDot || i + 1 >= argc ||
                std::string(argv[i + 1]).empty()) {
                return false;
            }
            options.shouldEmitDot = true;
            options.dotPath = argv[i + 1];
            i += 2;
            continue;
        }
        if (option == "--max-steps" || option == "--max-call-depth") {
            bool& supplied = option == "--max-steps" ? hasSteps : hasDepth;
            auto& value = option == "--max-steps" ? options.limits.maxSteps : options.limits.maxCallDepth;
            if (supplied || i + 1 >= argc || !parseLimit(argv[i + 1], value)) return false;
            supplied = true;
            i += 2;
            continue;
        }
        return false;
    }
    return options.shouldRun || (!hasSteps && !hasDepth);
}

static bool writeDotFile(
    const std::string& path,
    const Program& program,
    const std::vector<Violation>& violations) {
    try {
        std::ofstream output(std::filesystem::u8path(path), std::ios::binary);
        if (!output) return false;
        output << DependencyGraphExporter::toDot(program, violations);
        return output.good();
    } catch (const std::exception&) {
        return false;
    }
}

static void printExecutionTrace(const ExecutionResult& execution) {
    std::cout << "── 실행 Trace ──\n";
    for (const auto& event : execution.events) {
        const std::string indent(event.depth * 2, ' ');
        switch (event.kind) {
            case ExecutionEventKind::EnterFunction:
                std::cout << indent << "enter " << event.function << "\n";
                break;
            case ExecutionEventKind::CallFunction:
                std::cout << indent << "call " << event.target;
                if (event.line > 0) std::cout << " (" << event.line << "행)";
                std::cout << "\n";
                break;
            case ExecutionEventKind::ExitFunction:
                std::cout << indent << "exit " << event.function << "\n";
                break;
        }
    }
    std::cout << "\n✓ 실행 완료: 함수 " << execution.functionsExecuted
              << "회, 호출 " << execution.callsExecuted
              << "회 (functions_executed=" << execution.functionsExecuted
              << ", calls_executed=" << execution.callsExecuted << ")\n";
}

int main(int argc, char** argv) {
    std::vector<std::string> argumentStorage;
    std::vector<char*> argumentPointers;
    try {
        argumentStorage = utf8Arguments(argc, argv);
        for (auto& argument : argumentStorage) argumentPointers.push_back(argument.data());
        argc = static_cast<int>(argumentStorage.size());
        argumentPointers.push_back(nullptr);
        argv = argumentPointers.data();
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 2;
    }
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "ieum " << kIeumVersion << "\n";
        return 0;
    }

    CliOptions options;
    if (!parseCliOptions(argc, argv, options)) {
        printUsage();
        return 2;
    }

    // 1) 파일 읽기
    std::ifstream file(std::filesystem::u8path(options.sourcePath), std::ios::binary);
    if (!file) {
        std::cerr << "오류: 파일을 열 수 없습니다 — " << options.sourcePath << "\n";
        return 2;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    try {
        // 2) 렉싱 → 파싱
        Lexer lexer(source, options.sourcePath);
        Parser parser(lexer.tokenize());
        Program prog = parser.parse();

        // 3) 구조 요약 출력
        std::cout << "── 파싱 결과 ──\n";
        std::cout << "모듈 " << prog.modules.size()
                  << "개, 계층 선언 " << prog.layers.size() << "개"
                  << " (modules=" << prog.modules.size()
                  << ", layers=" << prog.layers.size() << ")\n\n";
        const BodySummary bodySummary = summarizeBodies(prog);
        if (bodySummary.modules > 0) {
            std::cout << "모듈 본문 " << bodySummary.modules
                      << "개, 모듈 변수 " << bodySummary.moduleVariables
                      << "개, 함수 " << bodySummary.functions
                      << "개, 함수 문장 " << bodySummary.functionStatements
                      << "개"
                      << " (body_modules=" << bodySummary.modules
                      << ", module_variables=" << bodySummary.moduleVariables
                      << ", functions=" << bodySummary.functions
                      << ", function_statements=" << bodySummary.functionStatements
                      << ")\n\n";
        }

        // 4) 의존 검사 (끌 수 없음 — 항상 전부 적용)
        Checker checker(prog);
        auto violations = checker.check();

        if (options.shouldEmitDot) {
            if (writeDotFile(options.dotPath, prog, violations)) {
                std::cout << "✓ 의존 그래프 저장: " << options.dotPath
                          << " (graph_format=dot)\n\n";
            } else {
                std::cerr << "경고: 의존 그래프를 저장할 수 없습니다 — "
                          << options.dotPath
                          << " (graph_export=failed, 구조 검사 결과에는 영향 없음)\n";
            }
        }

        if (!violations.empty()) {
            std::cout << "✗ 구조 검사 실패: 위반 " << violations.size() << "건\n\n";
            for (const auto& violation : violations) {
                std::cout << "  [" << structuralKindLabel(violation.kind) << "] "
                          << violation.message;
                if (violation.line > 0) {
                    std::cout << " (" << violation.line << "행)";
                }
                std::cout << "\n";
            }
            return 1;
        }
        std::cout << "✓ 구조 검사 통과: 위반 없음\n";

        // 5) 이름·Scope·함수 호출 의미 검사
        SemanticAnalyzer analyzer(prog);
        const SemanticResult semantics = analyzer.analyze();
        if (!semantics.ok()) {
            std::cout << "\n✗ 의미 검사 실패: 위반 "
                      << semantics.violations.size() << "건\n\n";
            for (const auto& violation : semantics.violations) {
                std::cout << "  [" << semanticKindLabel(violation.kind) << "] "
                          << violation.message;
                if (!violation.span.file.empty()) {
                    std::cout << " (" << sourceLocation(violation.span) << ")";
                }
                if (violation.line > 0) {
                    std::cout << " (" << violation.line << "행)";
                }
                std::cout << "\n";
            }
            return 1;
        }
        std::cout << "✓ 의미 검사 통과: 위반 없음\n";

        if (!options.shouldRun) return 0;

        // 6) 의미 검사를 통과한 값과 함수 호출 실행
        Interpreter interpreter(prog, semantics, options.limits);
        const ExecutionResult execution =
            interpreter.run(options.entryModule, options.entryFunction);
        if (!execution.success) {
            std::cout << "\n✗ 실행 실패: " << execution.error << "\n";
            return 1;
        }

        std::cout << "\n";
        printExecutionTrace(execution);
        if (valueType(execution.returnValue) != ValueType::Unit) {
            std::cout << "return " << options.entryModule << "." << options.entryFunction
                      << ": " << typeName(valueType(execution.returnValue))
                      << " = " << valueText(execution.returnValue) << "\n";
        }
        for (const auto& [name, value] : execution.moduleValues) {
            if (valueType(value) != ValueType::Unit) {
                std::cout << "value " << name << ": " << typeName(valueType(value))
                          << " = " << valueText(value) << "\n";
            }
        }
        for (const auto& [name, value] : execution.entryLocals) {
            if (valueType(value) != ValueType::Unit) {
                std::cout << "value " << options.entryModule << "." << options.entryFunction
                          << "." << name << ": " << typeName(valueType(value))
                          << " = " << valueText(value) << "\n";
            }
        }
        return 0;

    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
