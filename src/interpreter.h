#ifndef IEUM_INTERPRETER_H
#define IEUM_INTERPRETER_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include <map>

#include "ast.h"
#include "semantic.h"
#include "evaluator.h"

enum class ExecutionEventKind {
    EnterFunction,
    CallFunction,
    ExitFunction
};

struct ExecutionEvent {
    ExecutionEventKind kind;
    std::string function;
    std::string target;
    std::size_t depth;
    int line;
};

struct ExecutionResult {
    bool success = false;
    std::string error;
    std::vector<ExecutionEvent> events;
    std::size_t functionsExecuted = 0;
    std::size_t callsExecuted = 0;
    std::map<std::string, Value> moduleValues;
    std::map<std::string, Value> entryLocals;
};

class Interpreter {
public:
    Interpreter(const Program& program, const SemanticResult& semantics)
        : program_(program), semantics_(semantics) {}

    ExecutionResult run(
        const std::string& moduleName,
        const std::string& functionName) const {
        ExecutionResult result;
        if (!semantics_.ok()) {
            result.error = "의미 오류가 있는 프로그램은 실행할 수 없습니다";
            return result;
        }

        const auto entry = findFunction(moduleName, functionName, result);
        if (!entry.has_value()) return result;

        const auto& function =
            program_.modules[entry->module].functions[entry->function];
        if (!function.parameters.empty()) {
            result.error = "진입 함수 '" + qualifiedName(*entry) +
                "'는 매개변수가 없어야 합니다";
            return result;
        }

        result.success = true;
        std::vector<ValueScope> globals(program_.modules.size());
        try {
            for (std::size_t m = 0; m < program_.modules.size(); ++m) {
                for (const auto& variable : program_.modules[m].variables) {
                    const Value value = evaluateExpression(variable.initializer, {}, globals[m]);
                    globals[m].emplace(variable.name, value);
                }
            }
            execute(*entry, {}, globals, 0, result);
            if (result.success) {
                for (std::size_t m = 0; m < globals.size(); ++m) {
                    for (const auto& [name, value] : globals[m]) {
                        result.moduleValues.emplace(program_.modules[m].name + "." + name, value);
                    }
                }
            }
        } catch (const std::exception& error) {
            result.success = false;
            result.error = error.what();
        }
        return result;
    }

private:
    static constexpr std::size_t kMaxCallDepth = 1024;

    const Program& program_;
    const SemanticResult& semantics_;

    std::optional<FunctionRef> findFunction(
        const std::string& moduleName,
        const std::string& functionName,
        ExecutionResult& result) const {
        std::optional<std::size_t> moduleIndex;
        for (std::size_t i = 0; i < program_.modules.size(); ++i) {
            if (program_.modules[i].name != moduleName) continue;
            if (moduleIndex.has_value()) {
                result.error = "실행 모듈 '" + moduleName + "'이 중복 선언되었습니다";
                return std::nullopt;
            }
            moduleIndex = i;
        }

        if (!moduleIndex.has_value()) {
            result.error = "실행 모듈 '" + moduleName + "'을 찾을 수 없습니다";
            return std::nullopt;
        }

        std::optional<std::size_t> functionIndex;
        const auto& module = program_.modules[*moduleIndex];
        for (std::size_t i = 0; i < module.functions.size(); ++i) {
            if (module.functions[i].name != functionName) continue;
            if (functionIndex.has_value()) {
                result.error = "진입 함수 '" + moduleName + "." + functionName +
                    "'가 중복 선언되었습니다";
                return std::nullopt;
            }
            functionIndex = i;
        }

        if (!functionIndex.has_value()) {
            result.error = "진입 함수 '" + moduleName + "." + functionName +
                "'을 찾을 수 없습니다";
            return std::nullopt;
        }
        return FunctionRef{*moduleIndex, *functionIndex};
    }

    void execute(
        const FunctionRef& functionRef,
        const std::vector<Value>& arguments,
        std::vector<ValueScope>& globals,
        std::size_t depth,
        ExecutionResult& result) const {
        if (!result.success) return;
        if (depth > kMaxCallDepth) {
            result.success = false;
            result.error = "최대 함수 호출 깊이를 초과했습니다";
            return;
        }

        const auto& function =
            program_.modules[functionRef.module].functions[functionRef.function];
        const std::string name = qualifiedName(functionRef);
        ValueScope locals;
        for (std::size_t p = 0; p < function.parameters.size(); ++p) {
            locals.emplace(function.parameters[p], arguments.at(p));
        }
        auto& module = globals[functionRef.module];
        result.events.push_back({
            ExecutionEventKind::EnterFunction,
            name,
            "",
            depth,
            function.line
        });
        result.functionsExecuted++;

        for (std::size_t statementIndex = 0;
             statementIndex < function.body.size();
             ++statementIndex) {
            const auto& statement = function.body[statementIndex];
            if (statement.kind == Statement::Kind::VariableDeclaration) {
                const Value value = evaluateExpression(statement.expression, locals, module);
                locals.emplace(statement.name, value);
                continue;
            }
            if (statement.kind == Statement::Kind::Assignment) {
                Value value = evaluateExpression(statement.expression, locals, module);
                const auto local = locals.find(statement.name);
                if (local != locals.end()) local->second = std::move(value);
                else module.at(statement.name) = std::move(value);
                continue;
            }

            const auto* call = statement.id != 0 ? semantics_.findCallByNode(statement.id)
                                               : semantics_.findCall(functionRef, statementIndex);
            if (call == nullptr) {
                result.success = false;
                result.error = "해석되지 않은 호출이 실행 경로에 남아 있습니다";
                return;
            }

            const std::string target = qualifiedName(call->target);
            std::vector<Value> values;
            for (const auto& argument : statement.arguments) {
                values.push_back(lookupValue(argument, locals, module, statement.span));
            }
            result.events.push_back({
                ExecutionEventKind::CallFunction,
                name,
                target,
                depth,
                statement.line
            });
            result.callsExecuted++;
            execute(call->target, values, globals, depth + 1, result);
            if (!result.success) return;
        }

        if (depth == 0) result.entryLocals.insert(locals.begin(), locals.end());

        result.events.push_back({
            ExecutionEventKind::ExitFunction,
            name,
            "",
            depth,
            function.line
        });
    }

    std::string qualifiedName(const FunctionRef& ref) const {
        const auto& module = program_.modules[ref.module];
        return module.name + "." + module.functions[ref.function].name;
    }
};

#endif // IEUM_INTERPRETER_H
