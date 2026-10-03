#ifndef IEUM_SEMANTIC_H
#define IEUM_SEMANTIC_H

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ast.h"
#include "standard_library.h"
#include "module_order.h"

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
    MissingReturn, InvalidControlFlow, IncompleteSignature, InvalidType, InvalidField, InvalidAccess
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

struct FunctionSignature { std::vector<ValueType> parameters; ValueType result; };
struct RecordShape { std::vector<std::pair<std::string, ValueType>> fields; SourceSpan span; std::size_t module = 0; bool isPrivate = false; };
struct ModuleValueRef { std::size_t module; std::string name; };

struct SemanticResult {
    std::vector<SemanticViolation> violations;
    std::vector<ResolvedCall> resolvedCalls;
    std::vector<std::vector<FunctionSignature>> signatures;
    std::map<std::string, RecordShape> records;
    std::unordered_map<NodeId, ValueType> expressionTypes;
    std::unordered_map<NodeId, ModuleValueRef> resolvedValues;
    std::vector<std::unordered_map<std::string, ValueType>> moduleTypes;

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
        prepareTypes(result);
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

    static void typeError(SemanticResult& result, const SourceSpan& span, const std::string& message) {
        result.violations.push_back({SemanticViolationKind::InvalidType, message, span.line, span});
    }
    bool moduleAccess(std::size_t caller, std::size_t target, const SourceSpan& span, SemanticResult& result) const {
        if (caller == target) return true;
        const auto& name = program_.modules[target].name;
        const auto& deps = program_.modules[caller].deps;
        if (std::find(deps.begin(), deps.end(), name) != deps.end()) return true;
        result.violations.push_back({SemanticViolationKind::InvalidAccess, "missing_dependency: depends " + name + " 선언이 필요합니다", span.line, span});
        return false;
    }
    bool privateAccess(bool isPrivate, std::size_t caller, std::size_t target, const SourceSpan& span, SemanticResult& result) const {
        if (!isPrivate || caller == target) return true;
        result.violations.push_back({SemanticViolationKind::InvalidAccess, "private_access: 다른 모듈의 private 선언에 접근할 수 없습니다", span.line, span});
        return false;
    }
    void typeAccess(const ValueType& type, std::size_t caller, const SourceSpan& span, SemanticResult& result) const {
        if (type.kind == ValueType::List) { typeAccess(*type.element, caller, span, result); return; }
        if (type.kind != ValueType::Record) return;
        const auto found = result.records.find(type.name);
        if (found == result.records.end()) return;
        moduleAccess(caller, found->second.module, span, result);
        privateAccess(found->second.isPrivate, caller, found->second.module, span, result);
    }
    ValueType canonicalType(const ValueType& type, std::size_t module, const SourceSpan& span, SemanticResult& result) const {
        if (type.kind == ValueType::List) return ValueType::list(canonicalType(*type.element, module, span, result));
        if (type.kind != ValueType::Record) return type;
        if (type.name.find('.') != std::string::npos) {
            const auto record = result.records.find(type.name);
            if (record == result.records.end()) { typeError(result, span, "unknown_type: 타입을 찾을 수 없습니다: " + type.name); return ValueType::Unit; }
            typeAccess(type, module, span, result);
            return type;
        }
        const auto local = program_.modules[module].name + "." + type.name;
        if (result.records.count(local)) return ValueType::record(local);
        std::set<std::string> candidates;
        bool hidden = false;
        for (const auto& dep : program_.modules[module].deps) {
            const auto found = result.records.find(dep + "." + type.name);
            if (found == result.records.end()) continue;
            if (found->second.isPrivate) hidden = true;
            else candidates.insert(found->first);
        }
        if (candidates.size() == 1) {
            const auto resolved = ValueType::record(*candidates.begin()); typeAccess(resolved, module, span, result); return resolved;
        }
        if (candidates.empty() && hidden) {
            result.violations.push_back({SemanticViolationKind::InvalidAccess, "private_access: 의존 모듈의 private 타입에 접근할 수 없습니다: " + type.name, span.line, span});
            return ValueType::Unit;
        }
        typeError(result, span, candidates.empty() ? "unknown_type_or_dependency: 타입 또는 depends 선언을 찾을 수 없습니다: " + type.name
                                                  : "ambiguous_type: 여러 의존 모듈에 같은 타입이 있습니다: " + type.name);
        return ValueType::Unit;
    }
    std::optional<ValueType> annotationType(const std::optional<ValueType>& type, std::size_t module,
            const SourceSpan& span, SemanticResult& result) const {
        if (!type) return std::nullopt;
        return canonicalType(*type, module, span, result);
    }
    void prepareTypes(SemanticResult& result) const {
        for (std::size_t m = 0; m < program_.modules.size(); ++m) for (const auto& record : program_.modules[m].records) {
            const auto& module = program_.modules[m];
            if (record.name == "int" || record.name == "bool" || record.name == "string" || record.name == "unit" || record.name == "list") {
                typeError(result, record.span, "reserved_type: 기본 타입 이름을 record로 선언할 수 없습니다");
            }
            if (!result.records.emplace(module.name + "." + record.name, RecordShape{{}, record.span, m, record.isPrivate}).second) {
                typeError(result, record.span, "duplicate_record: 레코드가 중복 선언되었습니다");
            }
        }
        for (std::size_t m = 0; m < program_.modules.size(); ++m) {
            const auto& module = program_.modules[m];
            for (const auto& record : module.records) {
                auto& shape = result.records.at(module.name + "." + record.name);
                std::set<std::string> names;
                for (const auto& field : record.fields) {
                    if (!names.insert(field.first).second) typeError(result, record.span, "duplicate_field: 필드가 중복 선언되었습니다: " + field.first);
                    shape.fields.emplace_back(field.first, canonicalType(field.second, m, record.span, result));
                }
            }
            std::vector<FunctionSignature> signatures;
            for (const auto& fn : module.functions) {
                FunctionSignature signature;
                signature.result = canonicalType(fn.returnType, m, fn.span, result);
                for (std::size_t p = 0; p < fn.parameters.size(); ++p) signature.parameters.push_back(canonicalType(parameterType(fn, p), m, fn.span, result));
                signatures.push_back(std::move(signature));
            }
            result.signatures.push_back(std::move(signatures));
        }
        std::map<std::string, unsigned> visited, heights;
        std::function<unsigned(const std::string&, unsigned)> visit = [&](const std::string& name, unsigned depth) -> unsigned {
            auto& state = visited[name];
            if (state == 2) return heights[name];
            const auto& record = result.records.at(name);
            if (state == 1 || depth > 128) {
                typeError(result, record.span, "recursive_record: 순환 또는 과도하게 깊은 레코드 타입은 지원하지 않습니다");
                return 129;
            }
            state = 1;
            unsigned height = 1;
            for (const auto& field : record.fields) {
                auto type = field.second;
                while (type.kind == ValueType::List) { const auto element = *type.element; type = element; }
                if (type.kind == ValueType::Record) height = std::max(height, 1 + visit(type.name, depth + 1));
            }
            if (height > 128) typeError(result, record.span, "recursive_record: 레코드 타입 최대 중첩 깊이를 초과했습니다");
            state = 2;
            return heights[name] = std::min(height, 129u);
        };
        for (const auto& record : result.records) visit(record.first, 1);
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
                        variable.line, variable.span
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
                        function.line, function.span
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
                            function.line, function.span
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
        result.moduleTypes.resize(program_.modules.size());
        auto& moduleTypes = result.moduleTypes;
        for (const auto m : moduleInitializationOrder(program_)) {
            for (const auto& variable : program_.modules[m].variables) {
                const auto annotation = annotationType(variable.annotation, m, variable.span, result);
                const auto type = expressionType(variable.initializer, {}, moduleTypes[m], result, {m, noStatement}, annotation);
                checkType(annotation, type, variable.span, result);
                moduleTypes[m].emplace(variable.name, annotation.value_or(type.value_or(ValueType::Unit)));
            }
        }
        for (std::size_t m = 0; m < program_.modules.size(); ++m) {
            for (std::size_t f = 0; f < program_.modules[m].functions.size(); ++f) {
                const auto& function = program_.modules[m].functions[f];
                if (!function.native.empty()) continue;
                const auto& signature = result.signatures[m][f];
                TypeScopes locals(1);
                for (std::size_t p = 0; p < function.parameters.size(); ++p) locals.back().emplace(function.parameters[p], signature.parameters[p]);
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
                    const auto annotation = annotationType(statement.annotation, caller.module, statement.span, result);
                    const auto type = expressionType(statement.expression, locals, module, result, caller, annotation);
                    checkType(annotation, type, statement.span, result);
                    if (!locals.back().emplace(statement.name, annotation.value_or(type.value_or(ValueType::Unit))).second) {
                        result.violations.push_back({SemanticViolationKind::DuplicateLocalVariable,
                            "같은 블록의 지역 이름 '" + statement.name + "'가 중복 선언되었습니다", statement.line, statement.span});
                    }
                    break;
                }
                case Statement::Kind::Assignment: {
                    const auto expected = statement.target ? expressionType(statement.target, locals, module, result, caller)
                        : lookupType(statement.name, locals, module, statement.span, result);
                    const auto actual = expressionType(statement.expression, locals, module, result, caller, expected);
                    checkType(expected, actual, statement.span, result);
                    break;
                }
                case Statement::Kind::FunctionCall:
                    resolveTypedCall(caller, statement.name, statement.callArguments, statement.id, statement.span,
                                     locals, module, result, i);
                    break;
                case Statement::Kind::Return: {
                    const auto expected = result.signatures[caller.module][caller.function].result;
                    checkType(expected, expressionType(statement.expression, locals, module, result, caller, expected), statement.span, result);
                    next = Returns;
                    break;
                }
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
                case Statement::Kind::For: {
                    // The header lives for the whole loop; each body iteration has its own scope.
                    locals.emplace_back();
                    checkStatements(statement.initializer, locals, module, caller, loops, result);
                    if (statement.expression) {
                        checkType(ValueType::Bool, expressionType(statement.expression, locals, module, result, caller), statement.span, result);
                    }
                    checkStatements(statement.update, locals, module, caller, loops, result);
                    locals.emplace_back();
                    const auto bodyFlow = checkStatements(statement.body, locals, module, caller, loops + 1, result);
                    locals.pop_back();
                    locals.pop_back();
                    next = Fallthrough | (bodyFlow & Returns);
                    break;
                }
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
        const auto before = result.violations.size();
        const auto target = resolveFunction(caller.module, name, span.line, result);
        for (std::size_t i = before; i < result.violations.size(); ++i) result.violations[i].span = span;
        if (!target) {
            for (const auto& argument : arguments) expressionType(argument, locals, module, result, caller);
            return std::nullopt;
        }
        const auto& function = program_.modules[target->module].functions[target->function];
        const auto& signature = result.signatures[target->module][target->function];
        typeAccess(signature.result, caller.module, span, result);
        if (arguments.size() != function.parameters.size()) {
            result.violations.push_back({SemanticViolationKind::ArityMismatch,
                "함수 '" + qualifiedName(*target) + "'의 인자 개수가 일치하지 않습니다", span.line, span});
        }
        for (std::size_t p = 0; p < arguments.size(); ++p) {
            const bool anyList = function.native == "std_list.length" && p == 0;
            const std::optional<ValueType> expected = p < signature.parameters.size() && !anyList
                ? std::optional<ValueType>(signature.parameters[p]) : std::nullopt;
            if (expected) typeAccess(*expected, caller.module, arguments[p]->span, result);
            const auto actual = expressionType(arguments[p], locals, module, result, caller, expected);
            if (anyList && actual && actual->kind != ValueType::List) typeError(result, arguments[p]->span, "list_required: length에는 목록이 필요합니다");
            else checkType(expected, actual, arguments[p]->span, result);
        }
        result.resolvedCalls.push_back({caller, statement, *target, span.line, node});
        return signature.result;
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
                                          const TypeScope& module, SemanticResult& result, FunctionRef caller, std::optional<ValueType> expected = std::nullopt) const {
        if (!expr) return ValueType::Unit;
        if (expr->kind == Expression::Kind::Literal) return valueType(expr->literal);
        if (expr->kind == Expression::Kind::Name) return lookupType(expr->text, locals, module, expr->span, result);
        if (expr->kind == Expression::Kind::Call) return resolveTypedCall(caller, expr->text, expr->arguments, expr->id, expr->span, locals, module, result);
        if (expr->kind == Expression::Kind::List) {
            std::optional<ValueType> element;
            if (expected && expected->kind == ValueType::List) element = *expected->element;
            for (const auto& item : expr->arguments) {
                const auto actual = expressionType(item, locals, module, result, caller, element);
                if (!element) element = actual;
                else checkType(element, actual, item->span, result);
            }
            if (!element) { typeError(result, expr->span, "empty_list_type: 빈 목록에는 원소 타입이 필요합니다"); return std::nullopt; }
            auto type = ValueType::list(*element);
            auto inner = type; unsigned depth = 0;
            while (inner.kind == ValueType::List) { const auto next = *inner.element; inner = next; ++depth; }
            if (depth > 128) { typeError(result, expr->span, "type_depth_limit: 목록 타입 최대 중첩 깊이를 초과했습니다"); return std::nullopt; }
            result.expressionTypes[expr->id] = type;
            return type;
        }
        if (expr->kind == Expression::Kind::Index) {
            const auto base = expressionType(expr->left, locals, module, result, caller);
            checkType(ValueType::Int, expressionType(expr->right, locals, module, result, caller), expr->right->span, result);
            if (!base) return std::nullopt;
            if (base->kind != ValueType::List) { typeError(result, expr->span, "list_required: 인덱스 접근에는 목록이 필요합니다"); return std::nullopt; }
            return *base->element;
        }
        if (expr->kind == Expression::Kind::Field) {
            if (expr->left->kind == Expression::Kind::Name) {
                const auto& name = expr->left->text;
                bool variable = module.count(name) != 0;
                for (const auto& scope : locals) variable = variable || scope.count(name) != 0;
                if (!variable) {
                    const auto found = moduleByName_.find(name);
                    if (found == moduleByName_.end()) {
                        result.violations.push_back({SemanticViolationKind::InvalidAccess, "unknown_module: 모듈을 찾을 수 없습니다: " + name, expr->span.line, expr->span});
                        return std::nullopt;
                    }
                    const auto target = found->second;
                    if (!moduleAccess(caller.module, target, expr->span, result)) return std::nullopt;
                    for (const auto& declaration : program_.modules[target].variables) if (declaration.name == expr->text) {
                        if (!privateAccess(declaration.isPrivate, caller.module, target, expr->span, result)) return std::nullopt;
                        const auto type = result.moduleTypes[target].find(expr->text);
                        if (type == result.moduleTypes[target].end()) break;
                        typeAccess(type->second, caller.module, expr->span, result);
                        result.resolvedValues[expr->id] = {target, expr->text};
                        return type->second;
                    }
                    result.violations.push_back({SemanticViolationKind::UndefinedVariable, "undefined_module_value: 모듈 값이 없거나 아직 선언되지 않았습니다: " + name + "." + expr->text, expr->span.line, expr->span});
                    return std::nullopt;
                }
            }
            const auto base = expressionType(expr->left, locals, module, result, caller);
            if (!base) return std::nullopt;
            if (base->kind != ValueType::Record) { typeError(result, expr->span, "record_required: 필드 접근에는 레코드가 필요합니다"); return std::nullopt; }
            const auto record = result.records.find(base->name);
            if (record != result.records.end()) for (const auto& field : record->second.fields) if (field.first == expr->text) { typeAccess(field.second, caller.module, expr->span, result); return field.second; }
            result.violations.push_back({SemanticViolationKind::InvalidField, "unknown_field: 필드를 찾을 수 없습니다: " + expr->text, expr->span.line, expr->span});
            return std::nullopt;
        }
        if (expr->kind == Expression::Kind::Unary) {
            const auto type = expressionType(expr->right, locals, module, result, caller);
            const auto expected = expr->text == "!" ? ValueType::Bool : ValueType::Int;
            checkType(expected, type, expr->span, result);
            return type && *type == expected ? type : std::nullopt;
        }
        // Type-check both sides even when evaluation can short-circuit.
        const auto left = expressionType(expr->left, locals, module, result, caller);
        const auto right = expressionType(expr->right, locals, module, result, caller, left);
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
        const auto dot = name.find('.');
        if (dot != std::string::npos) {
            const SourceSpan span{"", line};
            const auto found = moduleByName_.find(name.substr(0, dot));
            if (found == moduleByName_.end()) {
                const auto prefix = name.substr(0, dot);
                result.violations.push_back({SemanticViolationKind::InvalidAccess,
                    isStandardModule(prefix) ? "missing_dependency: depends " + prefix + " 선언이 필요합니다" : "unknown_module: 모듈을 찾을 수 없습니다: " + prefix, line});
                return std::nullopt;
            }
            if (!moduleAccess(callerModule, found->second, span, result)) return std::nullopt;
            const auto candidates = candidatesInModule(found->second, name.substr(dot + 1));
            if (candidates.size() == 1) {
                const auto ref = candidates.front();
                if (!privateAccess(program_.modules[ref.module].functions[ref.function].isPrivate, callerModule, ref.module, span, result)) return std::nullopt;
                return ref;
            }
            if (candidates.empty()) result.violations.push_back({SemanticViolationKind::UndefinedFunction, "undefined_function: 함수를 찾을 수 없습니다: " + name, line});
            return std::nullopt;
        }
        const auto local = candidatesInModule(callerModule, name);
        if (local.size() == 1) return local.front();
        if (local.size() > 1) return std::nullopt; // DuplicateFunction already reports it.

        std::vector<FunctionRef> dependencies;
        std::vector<FunctionRef> privateDependencies;
        std::unordered_set<std::size_t> seenNodes;
        for (const auto& dependencyName : program_.modules[callerModule].deps) {
            const auto moduleIt = moduleByName_.find(dependencyName);
            if (moduleIt == moduleByName_.end()) continue;

            for (const auto& candidate : candidatesInModule(moduleIt->second, name)) {
                if (program_.modules[candidate.module].functions[candidate.function].isPrivate) { privateDependencies.push_back(candidate); continue; }
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
        if (!privateDependencies.empty()) {
            result.violations.push_back({SemanticViolationKind::InvalidAccess, "private_access: 의존 모듈의 private 함수에 접근할 수 없습니다: " + name, line});
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

        const auto standardModule = standardFunctionModule(name);
        if (!standardModule.empty()) {
            result.violations.push_back({SemanticViolationKind::MissingCallDependency,
                "내장 함수 '" + name + "'에는 depends " + standardModule + " 선언이 필요합니다", line});
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
