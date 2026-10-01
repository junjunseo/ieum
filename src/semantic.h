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
    TypeMismatch,
    MissingReturn, InvalidControlFlow, IncompleteSignature
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
        return result;
    }

private:
    const Program& program_;
    std::unordered_map<std::string, std::size_t> moduleByName_;
    std::vector<std::unordered_map<std::string, std::vector<std::size_t>>>
        functionsByModule_;
    std::vector<std::size_t> functionOffsets_;

    void buildIndexes() {
        moduleByName_.clear();
        functionsByModule_.clear();
        functionOffsets_.clear();

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

                if (function.returnType != ValueType::Unit) {
                    for (std::size_t p = 0; p < function.parameters.size(); ++p) {
                        if (p >= function.explicitParameterTypes.size() || !function.explicitParameterTypes[p]) {
                            result.violations.push_back({SemanticViolationKind::IncompleteSignature,
                                "non-unit 함수의 매개변수 타입을 명시해야 합니다: " + function.parameters[p],
                                function.line, function.span});
                        }
                    }
                }

            }
        }
    }

    void resolveCalls(SemanticResult& result) const {
        std::vector<TypeScope> moduleTypes(program_.modules.size());
        for (std::size_t m = 0; m < program_.modules.size(); ++m) {
            for (const auto& variable : program_.modules[m].variables) {
                const auto type = expressionType(variable.initializer, {}, moduleTypes[m], result, {m, noStatement});
                checkType(variable.annotation, type, variable.span, result);
                moduleTypes[m].emplace(variable.name, variable.annotation.value_or(type.value_or(ValueType::Unit)));
            }
        }
        for (std::size_t m = 0; m < program_.modules.size(); ++m) {
            for (std::size_t f = 0; f < program_.modules[m].functions.size(); ++f) {
                const auto& function = program_.modules[m].functions[f];
                TypeScopes locals(1);
                for (std::size_t p = 0; p < function.parameters.size(); ++p) locals.back().emplace(function.parameters[p], parameterType(function, p));
                const auto flow = checkStatements(function.body, locals, moduleTypes[m], {m,f}, 0, result);
                if (function.returnType != ValueType::Unit && (flow & Fallthrough)) {
                    result.violations.push_back({SemanticViolationKind::MissingReturn,
                        "missing_return: 함수 '" + qualifiedName({m,f}) + "'의 모든 경로에 반환값이 필요합니다",
                        function.line, function.span});
                }
            }
        }
    }

    using TypeScope = std::unordered_map<std::string, ValueType>;
    using TypeScopes = std::vector<TypeScope>;
    static constexpr std::size_t noStatement = static_cast<std::size_t>(-1);
    enum Flow { Fallthrough = 1, Returns = 2, Breaks = 4, Continues = 8 };

    unsigned checkStatements(const std::vector<Statement>& statements, TypeScopes& locals,
            const TypeScope& module, FunctionRef caller, unsigned loops, SemanticResult& result) const {
        unsigned flow = Fallthrough;
        for (std::size_t i = 0; i < statements.size(); ++i) {
            const auto& statement = statements[i];
            unsigned next = Fallthrough;
            switch (statement.kind) {
                case Statement::Kind::VariableDeclaration: {
                    const auto type = expressionType(statement.expression, locals, module, result, caller);
                    checkType(statement.annotation, type, statement.span, result);
                    if (!locals.back().emplace(statement.name, statement.annotation.value_or(type.value_or(ValueType::Unit))).second) {
                        result.violations.push_back({SemanticViolationKind::DuplicateLocalVariable,
                            "같은 블록의 지역 이름 '" + statement.name + "'가 중복 선언되었습니다", statement.line, statement.span});
                    }
                    break;
                }
                case Statement::Kind::Assignment: {
                    const auto expected = lookupType(statement.name, locals, module, statement.span, result);
                    const auto actual = expressionType(statement.expression, locals, module, result, caller);
                    checkType(expected, actual, statement.span, result);
                    break;
                }
                case Statement::Kind::FunctionCall:
                    resolveTypedCall(caller, statement.name, statement.callArguments, statement.id, statement.span,
                                     locals, module, result, i);
                    break;
                case Statement::Kind::Return:
                    checkType(program_.modules[caller.module].functions[caller.function].returnType,
                        expressionType(statement.expression, locals, module, result, caller), statement.span, result);
                    next = Returns;
                    break;
                case Statement::Kind::Break:
                case Statement::Kind::Continue:
                    if (loops == 0) result.violations.push_back({SemanticViolationKind::InvalidControlFlow,
                        "invalid_control_flow: break/continue는 반복문 안에서만 사용할 수 있습니다", statement.line, statement.span});
                    next = statement.kind == Statement::Kind::Break ? Breaks : Continues;
                    break;
                case Statement::Kind::Block:
                    locals.emplace_back();
                    next = checkStatements(statement.body, locals, module, caller, loops, result);
                    locals.pop_back();
                    break;
                case Statement::Kind::If:
                case Statement::Kind::While: {
                    checkType(ValueType::Bool, expressionType(statement.expression, locals, module, result, caller), statement.span, result);
                    locals.emplace_back();
                    const auto bodyFlow = checkStatements(statement.body, locals, module, caller,
                        loops + (statement.kind == Statement::Kind::While ? 1 : 0), result);
                    locals.pop_back();
                    if (statement.kind == Statement::Kind::If) {
                        locals.emplace_back();
                        next = bodyFlow | checkStatements(statement.alternative, locals, module, caller, loops, result);
                        locals.pop_back();
                    } else next = Fallthrough | (bodyFlow & Returns); // loops may execute zero times
                    break;
                }
            }
            // Still check unreachable statements for name/type/dependency errors.
            if (flow & Fallthrough) flow = (flow & ~Fallthrough) | next;
        }
        return flow;
    }

    std::optional<ValueType> resolveTypedCall(FunctionRef caller, const std::string& name,
            const std::vector<Expr>& arguments, NodeId node, const SourceSpan& span,
            const TypeScopes& locals, const TypeScope& module, SemanticResult& result,
            std::size_t statement = noStatement) const {
        std::vector<std::optional<ValueType>> types;
        for (const auto& argument : arguments) types.push_back(expressionType(argument, locals, module, result, caller));
        const auto before = result.violations.size();
        const auto target = resolveFunction(caller.module, name, span.line, result);
        for (std::size_t i = before; i < result.violations.size(); ++i) result.violations[i].span = span;
        if (!target) return std::nullopt;
        const auto& function = program_.modules[target->module].functions[target->function];
        if (arguments.size() != function.parameters.size()) {
            result.violations.push_back({SemanticViolationKind::ArityMismatch,
                "함수 '" + qualifiedName(*target) + "'의 인자 개수가 일치하지 않습니다", span.line, span});
        }
        for (std::size_t p = 0; p < std::min(types.size(), function.parameters.size()); ++p) checkType(parameterType(function,p), types[p], span, result);
        result.resolvedCalls.push_back({caller, statement, *target, span.line, node});
        return function.returnType;
    }

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
            const TypeScopes& locals, const TypeScope& module,
            const SourceSpan& span, SemanticResult& result) {
        for (auto scope = locals.rbegin(); scope != locals.rend(); ++scope) {
            const auto local = scope->find(name);
            if (local != scope->end()) return local->second;
        }
        const auto global = module.find(name);
        if (global != module.end()) return global->second;
        result.violations.push_back({SemanticViolationKind::UndefinedVariable,
            "변수 '" + name + "'가 현재 Scope에 선언되지 않았습니다", span.line, span});
        return std::nullopt;
    }

    std::optional<ValueType> expressionType(const Expr& expr, const TypeScopes& locals,
                                          const TypeScope& module, SemanticResult& result, FunctionRef caller) const {
        if (!expr) return ValueType::Unit;
        if (expr->kind == Expression::Kind::Literal) return valueType(expr->literal);
        if (expr->kind == Expression::Kind::Name) return lookupType(expr->text, locals, module, expr->span, result);
        if (expr->kind == Expression::Kind::Call) return resolveTypedCall(caller, expr->text, expr->arguments, expr->id, expr->span, locals, module, result);
        if (expr->kind == Expression::Kind::Unary) {
            const auto type = expressionType(expr->right, locals, module, result, caller);
            const auto expected = expr->text == "!" ? ValueType::Bool : ValueType::Int;
            checkType(expected, type, expr->span, result);
            return type && *type == expected ? type : std::nullopt;
        }
        // Type-check both sides even when evaluation can short-circuit.
        const auto left = expressionType(expr->left, locals, module, result, caller);
        const auto right = expressionType(expr->right, locals, module, result, caller);
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
