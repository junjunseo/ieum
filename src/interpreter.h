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
    Value returnValue;
    std::size_t stepsExecuted = 0;
    SourceSpan failureSpan = {};
    std::vector<std::pair<std::string, SourceSpan>> callStack;
};

struct ExecutionLimits {
    std::size_t maxSteps = 100000;
    std::size_t maxCallDepth = 1024;
};

class Interpreter {
public:
    Interpreter(const Program& program, const SemanticResult& semantics, ExecutionLimits limits = {}, RuntimeIO io = {})
        : program_(program), semantics_(semantics), limits_(limits), io_(io) {}

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

        if (limits_.maxSteps == 0 || limits_.maxCallDepth == 0) {
            result.error = "실행 한도는 0보다 커야 합니다";
            return result;
        }
        result.success = true;
        try {
            Machine machine(program_, semantics_, limits_, result, io_);
            machine.run(*entry);
        } catch (const std::exception& error) {
            result.success = false;
            result.error = error.what();
        }
        return result;
    }

private:
    const Program& program_;
    const SemanticResult& semantics_;
    ExecutionLimits limits_;
    RuntimeIO io_;

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
        if (module.functions[*functionIndex].isPrivate) {
            result.failureSpan = module.functions[*functionIndex].span;
            result.error = sourceLocation(result.failureSpan) + " private_access: private 함수는 외부 진입점으로 실행할 수 없습니다";
            return std::nullopt;
        }
        return FunctionRef{*moduleIndex, *functionIndex};
    }

    // AST continuations and call frames live on the heap: language recursion
    // never recursively enters the C++ evaluator or statement executor.
    class Machine {
        enum class Kind { Statement, Expression, Unary, Binary, Logical,
            Invoke, Entry, ExitFunction, Return, Drop, StoreLocal, StoreAssign,
            StoreGlobal, EnterScope, ExitScope, Branch, LoopTest, LoopChoice, ForUpdate, MakeList, ReadIndex, ReadField };
        struct Task {
            Kind kind;
            std::size_t module;
            SourceSpan span;
            const Expression* expression = nullptr;
            const Statement* statement = nullptr;
            const VariableDecl* variable = nullptr;
            const std::vector<Statement>* body = nullptr;
            NodeId node = 0;
            std::size_t count = 0;
            Task(Kind k, std::size_t m, SourceSpan s = {}) : kind(k), module(m), span(std::move(s)) {}
        };
        struct Frame {
            FunctionRef ref;
            std::vector<ValueScope> scopes;
            std::size_t valueBase;
            bool entry;
            SourceSpan callSite;
        };
        const Program& program;
        const SemanticResult& semantics;
        ExecutionLimits limits;
        RuntimeIO io;
        ExecutionResult& result;
        std::vector<ValueScope> globals;
        std::vector<Frame> frames;
        std::vector<Task> work;
        std::vector<Value> values;
        FunctionRef entry{};

        std::string name(FunctionRef ref) const {
            return program.modules[ref.module].name + "." + program.modules[ref.module].functions[ref.function].name;
        }
        Value pop() {
            if (values.empty()) throw std::runtime_error("실행 값 스택이 비어 있습니다");
            Value value = std::move(values.back()); values.pop_back(); return value;
        }
        Value& variable(const std::string& key, std::size_t module, const SourceSpan& span) {
            if (!frames.empty()) {
                for (auto scope = frames.back().scopes.rbegin(); scope != frames.back().scopes.rend(); ++scope) {
                    const auto it = scope->find(key);
                    if (it != scope->end()) return it->second;
                }
            }
            const auto it = globals[module].find(key);
            if (it != globals[module].end()) return it->second;
            throw EvaluationError(span, "uninitialized_variable: 초기화된 변수를 찾을 수 없습니다: " + key);
        }
        Value& moduleValue(const ModuleValueRef& ref, const SourceSpan& span) {
            const auto found = globals[ref.module].find(ref.name);
            if (found != globals[ref.module].end()) return found->second;
            throw EvaluationError(span, "uninitialized_variable: 초기화된 모듈 값이 없습니다: " + program.modules[ref.module].name + "." + ref.name);
        }
        void expression(const Expression* expr, std::size_t module, const SourceSpan& fallback = {}) {
            Task task(Kind::Expression, module, expr ? expr->span : fallback);
            task.expression = expr; work.push_back(std::move(task));
        }
        void statements(const std::vector<Statement>& body, std::size_t module) {
            for (auto it = body.rbegin(); it != body.rend(); ++it) {
                Task task(Kind::Statement, module, it->span); task.statement = &*it; work.push_back(std::move(task));
            }
        }
        void block(const std::vector<Statement>& body, const Task& source) {
            Task task(Kind::EnterScope, source.module, source.span); task.body = &body; work.push_back(std::move(task));
        }
        void call(NodeId node, const std::vector<Expr>& arguments, const Task& source) {
            Task invoke(Kind::Invoke, source.module, source.span);
            invoke.node = node; invoke.count = arguments.size(); work.push_back(std::move(invoke));
            for (auto it = arguments.rbegin(); it != arguments.rend(); ++it) expression(it->get(), source.module);
        }
        void enter(FunctionRef ref, std::vector<Value> args, bool isEntry, const Task& source) {
            if (frames.size() >= limits.maxCallDepth) throw EvaluationError(source.span, "call_depth_limit: 최대 함수 호출 깊이를 초과했습니다");
            const auto& function = program.modules[ref.module].functions[ref.function];
            if (function.parameters.size() != args.size()) throw EvaluationError(source.span, "인자 개수가 일치하지 않습니다");
            const auto depth = frames.size();
            if (!isEntry) {
                const auto caller = frames.empty() ? program.modules[source.module].name + ".<init>" : name(frames.back().ref);
                result.events.push_back({ExecutionEventKind::CallFunction, caller, name(ref), depth == 0 ? 0 : depth - 1, source.span.line});
                ++result.callsExecuted;
            }
            Frame frame{ref, std::vector<ValueScope>(1), values.size(), isEntry, source.span};
            if (function.native.empty()) for (std::size_t p = 0; p < args.size(); ++p) frame.scopes.back().emplace(function.parameters[p], std::move(args[p]));
            frames.push_back(std::move(frame));
            result.events.push_back({ExecutionEventKind::EnterFunction, name(ref), "", depth, function.line});
            ++result.functionsExecuted;
            if (!function.native.empty()) {
                if (function.native == "record") {
                    const auto& type = semantics.signatures[ref.module][ref.function].result;
                    const auto& shape = semantics.records.at(type.name);
                    std::vector<std::pair<std::string, Value>> fields;
                    for (std::size_t i = 0; i < args.size(); ++i) fields.emplace_back(shape.fields[i].first, std::move(args[i]));
                    leave(makeRecord(type.name, std::move(fields)));
                } else leave(executeStandardFunction(function.native, args, io));
                return;
            }
            work.emplace_back(Kind::ExitFunction, ref.module, function.span);
            statements(function.body, ref.module);
        }
        void leave(Value value) {
            const auto& frame = frames.back();
            const auto& function = program.modules[frame.ref.module].functions[frame.ref.function];
            if (valueType(value) != semantics.signatures[frame.ref.module][frame.ref.function].result) throw EvaluationError(function.span, "missing_return: 올바른 반환값이 필요합니다");
            if (frame.entry) {
                result.entryLocals.insert(frame.scopes.front().begin(), frame.scopes.front().end());
                result.returnValue = value;
            }
            values.resize(frame.valueBase);
            result.events.push_back({ExecutionEventKind::ExitFunction, name(frame.ref), "", frames.size() - 1, function.line});
            frames.pop_back();
            values.push_back(std::move(value));
        }
        void loopControl(bool isBreak, const SourceSpan& span) {
            while (!work.empty() && work.back().kind != Kind::ExitFunction) {
                if (work.back().kind == Kind::LoopTest || work.back().kind == Kind::ForUpdate) {
                    if (isBreak) work.pop_back();
                    return;
                }
                if (work.back().kind == Kind::ExitScope) frames.back().scopes.pop_back();
                work.pop_back();
            }
            throw EvaluationError(span, "invalid_control_flow: 반복문을 찾을 수 없습니다");
        }
        void evaluate(const Task& task) {
            const auto* expr = task.expression;
            if (!expr) { values.emplace_back(std::monostate{}); return; }
            const auto reference = semantics.resolvedValues.find(expr->id);
            if (reference != semantics.resolvedValues.end()) { values.push_back(moduleValue(reference->second, expr->span)); return; }
            if (expr->kind == Expression::Kind::Literal) { values.push_back(expr->literal); return; }
            if (expr->kind == Expression::Kind::Name) { values.push_back(variable(expr->text, task.module, expr->span)); return; }
            if (expr->kind == Expression::Kind::Call) { call(expr->id, expr->arguments, task); return; }
            Task apply(task);
            if (expr->kind == Expression::Kind::List) {
                apply.kind = Kind::MakeList; apply.count = expr->arguments.size(); work.push_back(apply);
                for (auto it = expr->arguments.rbegin(); it != expr->arguments.rend(); ++it) expression(it->get(), task.module);
                return;
            }
            if (expr->kind == Expression::Kind::Index || expr->kind == Expression::Kind::Field) {
                apply.kind = expr->kind == Expression::Kind::Index ? Kind::ReadIndex : Kind::ReadField;
                work.push_back(apply);
                if (expr->right) expression(expr->right.get(), task.module);
                expression(expr->left.get(), task.module); return;
            }
            if (expr->kind == Expression::Kind::Unary) {
                apply.kind = Kind::Unary; work.push_back(apply); expression(expr->right.get(), task.module); return;
            }
            apply.kind = expr->text == "&&" || expr->text == "||" ? Kind::Logical : Kind::Binary;
            work.push_back(apply);
            if (apply.kind == Kind::Binary) expression(expr->right.get(), task.module);
            expression(expr->left.get(), task.module);
        }
        std::vector<const Expression*> accessPath(const Expr& target) const {
            std::vector<const Expression*> path;
            const auto* item = target.get();
            while (item && (item->kind == Expression::Kind::Index || item->kind == Expression::Kind::Field)) {
                if (semantics.resolvedValues.count(item->id)) break;
                path.push_back(item); item = item->left.get();
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        Value& assignmentRoot(const Statement& statement, std::size_t module) {
            const auto* item = statement.target.get();
            while (item) {
                const auto reference = semantics.resolvedValues.find(item->id);
                if (reference != semantics.resolvedValues.end()) return moduleValue(reference->second, statement.span);
                item = item->left.get();
            }
            return variable(statement.name, module, statement.span);
        }
        static Value replacePath(const Value& base, const std::vector<const Expression*>& path,
                const std::vector<std::int64_t>& indices, std::size_t step, std::size_t& index, Value replacement) {
            if (step == path.size()) return replacement;
            const auto* access = path[step];
            if (access->kind == Expression::Kind::Index) {
                const auto position = indices.at(index++);
                const auto& old = indexValue(base, position);
                const auto& list = *std::get<std::shared_ptr<const ListValue>>(base);
                auto items = list.items;
                items[static_cast<std::size_t>(position)] = replacePath(old, path, indices, step + 1, index, std::move(replacement));
                return makeList(list.elementType, std::move(items));
            }
            const auto& record = *std::get<std::shared_ptr<const RecordValue>>(base);
            auto fields = record.fields;
            for (auto& field : fields) if (field.first == access->text) {
                field.second = replacePath(field.second, path, indices, step + 1, index, std::move(replacement));
                return makeRecord(record.name, std::move(fields));
            }
            throw std::runtime_error("unknown_field: 필드를 찾을 수 없습니다");
        }
        void statement(const Task& task) {
            const auto& stmt = *task.statement;
            Task next(task);
            switch (stmt.kind) {
                case Statement::Kind::VariableDeclaration:
                case Statement::Kind::Assignment:
                    next.kind = stmt.kind == Statement::Kind::Assignment ? Kind::StoreAssign : Kind::StoreLocal;
                    work.push_back(next); expression(stmt.expression.get(), task.module, stmt.span);
                    if (stmt.kind == Statement::Kind::Assignment) {
                        const auto path = accessPath(stmt.target);
                        for (auto it = path.rbegin(); it != path.rend(); ++it) if ((*it)->kind == Expression::Kind::Index) expression((*it)->right.get(), task.module);
                    }
                    break;
                case Statement::Kind::FunctionCall:
                    work.emplace_back(Kind::Drop, task.module, stmt.span);
                    call(stmt.id, stmt.callArguments, task); break;
                case Statement::Kind::Return:
                    next.kind = Kind::Return; work.push_back(next); expression(stmt.expression.get(), task.module, stmt.span); break;
                case Statement::Kind::Block: block(stmt.body, task); break;
                case Statement::Kind::If:
                    next.kind = Kind::Branch; work.push_back(next); expression(stmt.expression.get(), task.module); break;
                case Statement::Kind::While:
                    next.kind = Kind::LoopTest; work.push_back(next); break;
                case Statement::Kind::For:
                    frames.back().scopes.emplace_back();
                    work.emplace_back(Kind::ExitScope, task.module, stmt.span);
                    next.kind = Kind::LoopTest; work.push_back(next);
                    statements(stmt.initializer, task.module);
                    break;
                case Statement::Kind::Break: loopControl(true, stmt.span); break;
                case Statement::Kind::Continue: loopControl(false, stmt.span); break;
            }
        }
        void execute(const Task& task) {
            switch (task.kind) {
                case Kind::Statement: statement(task); break;
                case Kind::Expression: evaluate(task); break;
                case Kind::Unary: { auto right = pop(); values.push_back(unaryValue(task.expression->text, right)); break; }
                case Kind::Binary: {
                    auto right = pop(); auto left = pop();
                    values.push_back(binaryValue(task.expression->text, left, right)); break;
                }
                case Kind::Logical: {
                    auto left = pop();
                    const bool value = std::get<bool>(left);
                    if ((task.expression->text == "&&" && !value) || (task.expression->text == "||" && value)) values.push_back(left);
                    else expression(task.expression->right.get(), task.module);
                    break;
                }
                case Kind::Invoke: {
                    const auto* resolved = semantics.findCallByNode(task.node);
                    if (!resolved || values.size() < task.count) throw EvaluationError(task.span, "해석되지 않은 호출입니다");
                    std::vector<Value> args(task.count);
                    for (std::size_t i = task.count; i > 0; --i) args[i-1] = pop();
                    enter(resolved->target, std::move(args), false, task); break;
                }
                case Kind::Entry: enter(entry, {}, true, task); break;
                case Kind::ExitFunction: leave(std::monostate{}); break;
                case Kind::Return: {
                    auto value = pop();
                    while (!work.empty() && work.back().kind != Kind::ExitFunction) work.pop_back();
                    if (work.empty()) throw EvaluationError(task.span, "반환할 함수를 찾을 수 없습니다");
                    work.pop_back(); leave(std::move(value)); break;
                }
                case Kind::Drop: pop(); break;
                case Kind::StoreLocal: frames.back().scopes.back().emplace(task.statement->name, pop()); break;
                case Kind::StoreAssign: {
                    auto value = pop();
                    const auto path = accessPath(task.statement->target);
                    std::vector<std::int64_t> indices;
                    for (const auto* access : path) if (access->kind == Expression::Kind::Index) indices.push_back(0);
                    for (std::size_t i = indices.size(); i > 0; --i) indices[i - 1] = std::get<std::int64_t>(pop());
                    auto& base = assignmentRoot(*task.statement, task.module);
                    std::size_t index = 0;
                    base = replacePath(base, path, indices, 0, index, std::move(value));
                    break;
                }
                case Kind::MakeList: {
                    std::vector<Value> items(task.count);
                    for (std::size_t i = task.count; i > 0; --i) items[i-1] = pop();
                    values.push_back(makeList(*semantics.expressionTypes.at(task.expression->id).element, std::move(items)));
                    break;
                }
                case Kind::ReadIndex: {
                    const auto index = std::get<std::int64_t>(pop());
                    const auto base = pop(); values.push_back(indexValue(base, index)); break;
                }
                case Kind::ReadField: {
                    const auto base = pop(); values.push_back(fieldValue(base, task.expression->text)); break;
                }
                case Kind::StoreGlobal: globals[task.module].emplace(task.variable->name, pop()); break;
                case Kind::EnterScope:
                    frames.back().scopes.emplace_back();
                    work.emplace_back(Kind::ExitScope, task.module, task.span);
                    statements(*task.body, task.module); break;
                case Kind::ExitScope: frames.back().scopes.pop_back(); break;
                case Kind::Branch:
                    block(std::get<bool>(pop()) ? task.statement->body : task.statement->alternative, task); break;
                case Kind::LoopTest: {
                    auto next = task; next.kind = Kind::LoopChoice; work.push_back(next);
                    if (task.statement->kind == Statement::Kind::For && !task.statement->expression) values.emplace_back(true);
                    else expression(task.statement->expression.get(), task.module);
                    break;
                }
                case Kind::LoopChoice:
                    if (std::get<bool>(pop())) {
                        auto next = task;
                        // Continue keeps this marker, so a for update runs before the next condition.
                        next.kind = task.statement->kind == Statement::Kind::For ? Kind::ForUpdate : Kind::LoopTest;
                        work.push_back(next);
                        block(task.statement->body, task);
                    }
                    break;
                case Kind::ForUpdate: {
                    auto next = task; next.kind = Kind::LoopTest; work.push_back(next);
                    statements(task.statement->update, task.module);
                    break;
                }
            }
        }
    public:
        Machine(const Program& p, const SemanticResult& s, ExecutionLimits l, ExecutionResult& r, RuntimeIO streams)
            : program(p), semantics(s), limits(l), io(streams), result(r), globals(p.modules.size()) {}
        void run(FunctionRef target) {
            entry = target;
            work.emplace_back(Kind::Entry, target.module, program.modules[target.module].functions[target.function].span);
            const auto order = moduleInitializationOrder(program);
            for (auto next = order.rbegin(); next != order.rend(); ++next) {
                const auto m = *next;
                const auto& module = program.modules[m];
                for (auto it = module.variables.rbegin(); it != module.variables.rend(); ++it) {
                    Task store(Kind::StoreGlobal, m, it->span); store.variable = &*it; work.push_back(store);
                    expression(it->initializer.get(), m, it->span);
                }
            }
            while (!work.empty()) {
                Task task = std::move(work.back()); work.pop_back();
                try {
                    if (result.stepsExecuted >= limits.maxSteps) throw EvaluationError(task.span, "step_limit: 최대 실행 스텝을 초과했습니다");
                    ++result.stepsExecuted;
                    execute(task);
                } catch (const std::exception& error) {
                    const auto* located = dynamic_cast<const SourceError*>(&error);
                    result.failureSpan = located ? located->span : task.span;
                    for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame) result.callStack.emplace_back(name(frame->ref), frame->callSite);
                    if (located) throw;
                    throw EvaluationError(task.span, error.what());
                }
            }
            for (std::size_t m = 0; m < globals.size(); ++m) {
                for (const auto& [key, value] : globals[m]) result.moduleValues.emplace(program.modules[m].name + "." + key, value);
            }
        }
    };

    std::string qualifiedName(const FunctionRef& ref) const {
        const auto& module = program_.modules[ref.module];
        return module.name + "." + module.functions[ref.function].name;
    }
};

#endif // IEUM_INTERPRETER_H
