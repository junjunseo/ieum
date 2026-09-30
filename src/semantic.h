#ifndef IEUM_SEMANTIC_H
#define IEUM_SEMANTIC_H

#include <algorithm>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ast.h"

enum class SemanticViolationKind {
    DuplicateModuleVariable,
    DuplicateFunction,
    DuplicateParameter,
    DuplicateLocalVariable,
    UndefinedVariable,
    UndefinedFunction,
    AmbiguousFunction,
    MissingCallDependency,
    ArityMismatch,
    RecursiveCall,
    TypeMismatch
};

struct SemanticViolation {
    SemanticViolationKind kind;
    std::string message;
    int line;
    SourceSpan span = {};
};

struct FunctionRef {
    std::size_t module;
    std::size_t function;

    bool operator==(const FunctionRef& other) const {
        return module == other.module && function == other.function;
    }
};

struct ResolvedCall {
    FunctionRef caller;
    std::size_t statement;
    FunctionRef target;
    int line;
    NodeId node = 0;
};

struct SemanticResult {
    std::vector<SemanticViolation> violations;
    std::vector<ResolvedCall> resolvedCalls;

    bool ok() const { return violations.empty(); }

    const ResolvedCall* findCallByNode(NodeId node) const {
        for (const auto& call : resolvedCalls) {
            if (node != 0 && call.node == node) return &call;
        }
        return nullptr;
    }

    const ResolvedCall* findCall(
        const FunctionRef& caller,
        std::size_t statement) const {
        for (const auto& call : resolvedCalls) {
            if (call.caller == caller && call.statement == statement) {
                return &call;
            }
        }
        return nullptr;
    }
};

class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(const Program& program)
        : program_(program) {}

    SemanticResult analyze() {
        buildIndexes();

        SemanticResult result;
        checkDeclarations(result);
        resolveCalls(result);
        checkRecursiveCalls(result);
        return result;
    }

private:
    const Program& program_;
    std::unordered_map<std::string, std::size_t> moduleByName_;
    std::vector<std::unordered_map<std::string, std::vector<std::size_t>>>
        functionsByModule_;
    std::vector<std::size_t> functionOffsets_;
    std::vector<FunctionRef> functionByNode_;

    void buildIndexes() {
        moduleByName_.clear();
        functionsByModule_.clear();
        functionOffsets_.clear();
        functionByNode_.clear();

        functionsByModule_.resize(program_.modules.size());
        functionOffsets_.resize(program_.modules.size());

        std::size_t node = 0;
        for (std::size_t moduleIndex = 0;
             moduleIndex < program_.modules.size();
             ++moduleIndex) {
            const auto& module = program_.modules[moduleIndex];
            moduleByName_.emplace(module.name, moduleIndex);
            functionOffsets_[moduleIndex] = node;

            for (std::size_t functionIndex = 0;
                 functionIndex < module.functions.size();
                 ++functionIndex) {
                functionsByModule_[moduleIndex][module.functions[functionIndex].name]
                    .push_back(functionIndex);
                functionByNode_.push_back({moduleIndex, functionIndex});
                node++;
            }
        }
    }

    void checkDeclarations(SemanticResult& result) const {
        for (const auto& module : program_.modules) {
            std::unordered_map<std::string, int> firstModuleVariable;
            for (const auto& variable : module.variables) {
                const auto [it, inserted] =
                    firstModuleVariable.emplace(variable.name, variable.line);
                if (!inserted) {
                    result.violations.push_back({
                        SemanticViolationKind::DuplicateModuleVariable,
                        "모듈 '" + module.name + "'의 변수 '" + variable.name +
                            "'가 중복 선언되었습니다 (최초 선언: " +
                            std::to_string(it->second) + "행)",
                        variable.line
                    });
                }
            }

            std::unordered_map<std::string, int> firstFunction;
            for (const auto& function : module.functions) {
                const auto [functionIt, functionInserted] =
                    firstFunction.emplace(function.name, function.line);
                if (!functionInserted) {
                    result.violations.push_back({
                        SemanticViolationKind::DuplicateFunction,
                        "모듈 '" + module.name + "'의 함수 '" + function.name +
                            "'가 중복 선언되었습니다 (최초 선언: " +
                            std::to_string(functionIt->second) + "행)",
                        function.line
                    });
                }

                std::unordered_map<std::string, int> functionScope;
                for (const auto& parameter : function.parameters) {
                    const bool inserted =
                        functionScope.emplace(parameter, function.line).second;
                    if (!inserted) {
                        result.violations.push_back({
                            SemanticViolationKind::DuplicateParameter,
                            "함수 '" + module.name + "." + function.name +
                                "'의 매개변수 '" + parameter +
                                "'가 중복 선언되었습니다",
                            function.line
                        });
                    }
                }

                for (const auto& statement : function.body) {
                    if (statement.kind != Statement::Kind::VariableDeclaration) {
                        continue;
                    }

                    const auto [it, inserted] =
                        functionScope.emplace(statement.name, statement.line);
                    if (!inserted) {
                        result.violations.push_back({
                            SemanticViolationKind::DuplicateLocalVariable,
                            "함수 '" + module.name + "." + function.name +
                                "'의 지역 이름 '" + statement.name +
                                "'가 중복 선언되었습니다 (최초 선언: " +
                                std::to_string(it->second) + "행)",
                            statement.line
                        });
                    }
                }
            }
        }
    }

    void resolveCalls(SemanticResult& result) const {
        for (std::size_t moduleIndex = 0;
             moduleIndex < program_.modules.size();
             ++moduleIndex) {
            const auto& module = program_.modules[moduleIndex];
            TypeScope moduleVariables;
            for (const auto& variable : module.variables) {
                const auto type = expressionType(variable.initializer, {}, moduleVariables, result);
                checkType(variable.annotation, type, variable.span, result);
                moduleVariables.emplace(variable.name, variable.annotation.value_or(type.value_or(ValueType::Unit)));
            }

            for (std::size_t functionIndex = 0;
                 functionIndex < module.functions.size();
                 ++functionIndex) {
                const auto& function = module.functions[functionIndex];
                TypeScope functionScope;
                for (std::size_t p = 0; p < function.parameters.size(); ++p) {
                    functionScope.emplace(function.parameters[p], parameterType(function, p));
                }

                for (std::size_t statementIndex = 0;
                     statementIndex < function.body.size();
                     ++statementIndex) {
                    const auto& statement = function.body[statementIndex];
                    if (statement.kind == Statement::Kind::VariableDeclaration) {
                        const auto type = expressionType(statement.expression, functionScope, moduleVariables, result);
                        checkType(statement.annotation, type, statement.span, result);
                        functionScope.emplace(statement.name, statement.annotation.value_or(type.value_or(ValueType::Unit)));
                        continue;
                    }
                    if (statement.kind == Statement::Kind::Assignment) {
                        const auto target = lookupType(statement.name, functionScope, moduleVariables, statement.span, result);
                        const auto type = expressionType(statement.expression, functionScope, moduleVariables, result);
                        checkType(target, type, statement.span, result);
                        continue;
                    }

                    std::vector<std::optional<ValueType>> argumentTypes;
                    for (const auto& argument : statement.arguments) {
                        argumentTypes.push_back(lookupType(argument, functionScope, moduleVariables, statement.span, result));
                    }

                    const auto target = resolveFunction(
                        moduleIndex, statement.name, statement.line, result);
                    if (!target.has_value()) continue;

                    const auto& targetFunction =
                        program_.modules[target->module].functions[target->function];
                    if (targetFunction.parameters.size() != statement.arguments.size()) {
                        result.violations.push_back({
                            SemanticViolationKind::ArityMismatch,
                            "함수 '" + qualifiedName(*target) + "'는 인자 " +
                                std::to_string(targetFunction.parameters.size()) +
                                "개가 필요하지만 " +
                                std::to_string(statement.arguments.size()) +
                                "개를 받았습니다",
                            statement.line
                        });
                    }
                    for (std::size_t p = 0; p < std::min(argumentTypes.size(), targetFunction.parameters.size()); ++p) {
                        checkType(parameterType(targetFunction, p), argumentTypes[p], statement.span, result);
                    }

                    result.resolvedCalls.push_back({
                        {moduleIndex, functionIndex},
                        statementIndex,
                        *target,
                        statement.line,
                        statement.id
                    });
                }
            }
        }
    }

    using TypeScope = std::unordered_map<std::string, ValueType>;

    static ValueType parameterType(const FunctionDecl& function, std::size_t p) {
        return p < function.parameterTypes.size() ? function.parameterTypes[p] : ValueType::Unit;
    }

    static void checkType(std::optional<ValueType> expected, std::optional<ValueType> actual,
                          const SourceSpan& span, SemanticResult& result) {
        if (expected && actual && *expected != *actual) {
            result.violations.push_back({SemanticViolationKind::TypeMismatch,
                "type_mismatch: " + typeName(*expected) + " 타입이 필요하지만 " + typeName(*actual) + "입니다",
                span.line, span});
        }
    }

    static std::optional<ValueType> lookupType(const std::string& name,
            const TypeScope& locals, const TypeScope& module,
            const SourceSpan& span, SemanticResult& result) {
        const auto local = locals.find(name);
        if (local != locals.end()) return local->second;
        const auto global = module.find(name);
        if (global != module.end()) return global->second;
        result.violations.push_back({SemanticViolationKind::UndefinedVariable,
            "변수 '" + name + "'가 현재 Scope에 선언되지 않았습니다", span.line, span});
        return std::nullopt;
    }

    std::optional<ValueType> expressionType(const Expr& expr, const TypeScope& locals,
                                          const TypeScope& module, SemanticResult& result) const {
        if (!expr) return ValueType::Unit;
        if (expr->kind == Expression::Kind::Literal) return valueType(expr->literal);
        if (expr->kind == Expression::Kind::Name) return lookupType(expr->text, locals, module, expr->span, result);
        if (expr->kind == Expression::Kind::Unary) {
            const auto type = expressionType(expr->right, locals, module, result);
            const auto expected = expr->text == "!" ? ValueType::Bool : ValueType::Int;
            checkType(expected, type, expr->span, result);
            return type && *type == expected ? type : std::nullopt;
        }
        // Type-check both sides even when evaluation can short-circuit.
        const auto left = expressionType(expr->left, locals, module, result);
        const auto right = expressionType(expr->right, locals, module, result);
        if (!left || !right) return std::nullopt;
        const auto& op = expr->text;
        if (*left == *right) {
            if (op == "==" || op == "!=") return ValueType::Bool;
            if (op == "+" && *left == ValueType::String) return ValueType::String;
            if ((op == "&&" || op == "||") && *left == ValueType::Bool) return ValueType::Bool;
            if (*left == ValueType::Int) {
                if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") return ValueType::Int;
                if (op == "<" || op == "<=" || op == ">" || op == ">=") return ValueType::Bool;
            }
        }
        result.violations.push_back({SemanticViolationKind::TypeMismatch,
            "type_mismatch: 연산 '" + op + "'에 " + typeName(*left) + ", " + typeName(*right) + " 타입을 사용할 수 없습니다",
            expr->span.line, expr->span});
        return std::nullopt;
    }

    std::optional<FunctionRef> resolveFunction(
        std::size_t callerModule,
        const std::string& name,
        int line,
        SemanticResult& result) const {
        const auto local = candidatesInModule(callerModule, name);
        if (local.size() == 1) return local.front();
        if (local.size() > 1) return std::nullopt; // DuplicateFunction already reports it.

        std::vector<FunctionRef> dependencies;
        std::unordered_set<std::size_t> seenNodes;
        for (const auto& dependencyName : program_.modules[callerModule].deps) {
            const auto moduleIt = moduleByName_.find(dependencyName);
            if (moduleIt == moduleByName_.end()) continue;

            for (const auto& candidate : candidatesInModule(moduleIt->second, name)) {
                const std::size_t node = functionNode(candidate);
                if (seenNodes.insert(node).second) dependencies.push_back(candidate);
            }
        }

        if (dependencies.size() == 1) return dependencies.front();
        if (dependencies.size() > 1) {
            result.violations.push_back({
                SemanticViolationKind::AmbiguousFunction,
                "호출 '" + name + "'이 여러 의존 모듈의 함수와 일치합니다: " +
                    joinNames(dependencies),
                line
            });
            return std::nullopt;
        }

        std::vector<FunctionRef> outsideDependencies;
        for (std::size_t moduleIndex = 0;
             moduleIndex < program_.modules.size();
             ++moduleIndex) {
            if (moduleIndex == callerModule) continue;
            const auto candidates = candidatesInModule(moduleIndex, name);
            outsideDependencies.insert(
                outsideDependencies.end(), candidates.begin(), candidates.end());
        }

        if (outsideDependencies.size() == 1) {
            const auto& targetModule =
                program_.modules[outsideDependencies.front().module].name;
            result.violations.push_back({
                SemanticViolationKind::MissingCallDependency,
                "함수 '" + name + "'은 모듈 '" + targetModule +
                    "'에 있지만 호출 모듈 '" +
                    program_.modules[callerModule].name +
                    "'의 depends 목록에 없습니다",
                line
            });
            return std::nullopt;
        }

        if (outsideDependencies.size() > 1) {
            result.violations.push_back({
                SemanticViolationKind::AmbiguousFunction,
                "호출 '" + name + "'이 여러 모듈의 함수와 일치하지만 "
                    "호출 모듈의 depends로 대상을 결정할 수 없습니다: " +
                    joinNames(outsideDependencies),
                line
            });
            return std::nullopt;
        }

        result.violations.push_back({
            SemanticViolationKind::UndefinedFunction,
            "함수 '" + name + "'을 현재 모듈이나 의존 모듈에서 찾을 수 없습니다",
            line
        });
        return std::nullopt;
    }

    std::vector<FunctionRef> candidatesInModule(
        std::size_t moduleIndex,
        const std::string& name) const {
        std::vector<FunctionRef> candidates;
        const auto it = functionsByModule_[moduleIndex].find(name);
        if (it == functionsByModule_[moduleIndex].end()) return candidates;

        for (const auto functionIndex : it->second) {
            candidates.push_back({moduleIndex, functionIndex});
        }
        return candidates;
    }

    void checkRecursiveCalls(SemanticResult& result) const {
        using Edge = std::pair<std::size_t, int>;
        std::vector<std::vector<Edge>> graph(functionByNode_.size());
        for (const auto& call : result.resolvedCalls) {
            graph[functionNode(call.caller)].push_back({
                functionNode(call.target), call.line
            });
        }

        std::vector<int> color(functionByNode_.size(), 0);
        std::vector<std::size_t> stack;
        std::unordered_set<std::string> reported;

        std::function<void(std::size_t)> visit = [&](std::size_t node) {
            color[node] = 1;
            stack.push_back(node);

            for (const auto& [target, line] : graph[node]) {
                if (color[target] == 0) {
                    visit(target);
                    continue;
                }
                if (color[target] != 1) continue;

                const auto cycleStart =
                    std::find(stack.begin(), stack.end(), target);
                if (cycleStart == stack.end()) continue;

                std::ostringstream path;
                for (auto it = cycleStart; it != stack.end(); ++it) {
                    if (it != cycleStart) path << " -> ";
                    path << qualifiedName(functionByNode_[*it]);
                }
                path << " -> " << qualifiedName(functionByNode_[target]);

                if (reported.insert(path.str()).second) {
                    result.violations.push_back({
                        SemanticViolationKind::RecursiveCall,
                        "종료 조건이 없는 재귀 호출 경로입니다: " + path.str(),
                        line
                    });
                }
            }

            stack.pop_back();
            color[node] = 2;
        };

        for (std::size_t node = 0; node < graph.size(); ++node) {
            if (color[node] == 0) visit(node);
        }
    }

    std::size_t functionNode(const FunctionRef& ref) const {
        return functionOffsets_[ref.module] + ref.function;
    }

    std::string qualifiedName(const FunctionRef& ref) const {
        const auto& module = program_.modules[ref.module];
        return module.name + "." + module.functions[ref.function].name;
    }

    std::string joinNames(const std::vector<FunctionRef>& functions) const {
        std::ostringstream names;
        for (std::size_t i = 0; i < functions.size(); ++i) {
            if (i > 0) names << ", ";
            names << qualifiedName(functions[i]);
        }
        return names.str();
    }
};

#endif // IEUM_SEMANTIC_H
