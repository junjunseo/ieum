#ifndef IEUM_PARSER_H
#define IEUM_PARSER_H

#include <stdexcept>
#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "token.h"
#include "ast.h"

// ── 파서 ───────────────────────────────────────────────
// 토큰 스트림(렉서 출력)을 받아 Program AST를 만든다.
// 전체 문법은 docs/GRAMMAR.md에서 관리한다.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens)
        : tokens_(std::move(tokens)) {}

    Program parse() {
        Program prog;
        while (!isAtEnd()) {
            // 빈 줄은 건너뛴다
            if (check(TokenType::NEWLINE)) { advance(); continue; }

            if (check(TokenType::MODULE)) {
                prog.modules.push_back(parseModule());
            } else if (check(TokenType::LAYER)) {
                prog.layers.push_back(parseLayer());
            } else {
                throw error("선언은 'module' 또는 'layer'로 시작해야 합니다");
            }
            consumeLineEnd();
        }
        return prog;
    }

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;
    NodeId nextId_ = 1;
    std::size_t expressionDepth_ = 0;
    std::size_t statementDepth_ = 0;

    // ── 선언 파싱 ──────────────────────────────────────
    ModuleDecl parseModule() {
        Token kw = advance();                 // MODULE
        Token name = expect(TokenType::IDENTIFIER,
                            "module 다음에는 모듈 이름이 와야 합니다");
        ModuleDecl decl;
        decl.name = name.value;
        decl.line = kw.line;
        decl.span = kw.span;

        if (check(TokenType::DEPENDS)) {
            advance();                        // DEPENDS
            decl.deps.push_back(
                expect(TokenType::IDENTIFIER,
                       "depends 다음에는 의존 대상 이름이 와야 합니다").value);
            while (check(TokenType::COMMA)) {
                advance();                    // COMMA
                decl.deps.push_back(
                    expect(TokenType::IDENTIFIER,
                           "',' 다음에는 의존 대상 이름이 와야 합니다").value);
            }
        }

        if (check(TokenType::LEFT_BRACE)) {
            decl.hasBody = true;
            advance();
            parseModuleBody(decl);
        }
        return decl;
    }

    void parseModuleBody(ModuleDecl& module) {
        if (check(TokenType::RIGHT_BRACE)) {
            advance();
            return;
        }

        consumeBlockStart("모듈 본문의 여는 중괄호 뒤에는 줄바꿈이 필요합니다");
        skipNewlines();

        while (!check(TokenType::RIGHT_BRACE)) {
            if (isAtEnd()) {
                throw error("모듈 본문을 닫는 '}'가 필요합니다");
            }

            if (check(TokenType::LET)) {
                module.variables.push_back(parseVariable());
            } else if (check(TokenType::FN)) {
                module.functions.push_back(parseFunction());
            } else {
                throw error("모듈 본문에는 'let' 또는 'fn' 선언만 올 수 있습니다");
            }

            consumeBlockMemberEnd("모듈 본문의 선언 뒤에는 줄바꿈이 필요합니다");
            skipNewlines();
        }

        advance(); // RIGHT_BRACE
    }

    VariableDecl parseVariable() {
        Token kw = advance(); // LET
        Token name = expect(TokenType::IDENTIFIER,
                            "let 다음에는 변수 이름이 와야 합니다");
        VariableDecl variable;
        variable.name = name.value;
        variable.line = kw.line;
        variable.span = kw.span;
        variable.id = nextId_++;
        if (check(TokenType::COLON)) { advance(); variable.annotation = parseType(); }
        if (check(TokenType::ASSIGN)) { advance(); variable.initializer = parseExpression(); }
        return variable;
    }

    FunctionDecl parseFunction() {
        Token kw = advance(); // FN
        Token name = expect(TokenType::IDENTIFIER,
                            "fn 다음에는 함수 이름이 와야 합니다");
        expect(TokenType::LEFT_PAREN, "함수 이름 뒤에는 '('가 필요합니다");

        FunctionDecl function;
        function.name = name.value;
        function.line = kw.line;
        function.span = kw.span;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                function.parameters.push_back(expect(TokenType::IDENTIFIER, "함수 매개변수 이름이 필요합니다").value);
                ValueType type = ValueType::Unit;
                function.explicitParameterTypes.push_back(check(TokenType::COLON));
                if (check(TokenType::COLON)) { advance(); type = parseType(); }
                function.parameterTypes.push_back(type);
                if (!check(TokenType::COMMA)) break;
                advance();
            } while (true);
        }
        expect(TokenType::RIGHT_PAREN, "함수 매개변수 목록을 닫는 ')'가 필요합니다");
        if (check(TokenType::ARROW)) { advance(); function.returnType = parseType(); }
        expect(TokenType::LEFT_BRACE, "함수 본문을 여는 '{'가 필요합니다");
        function.body = parseBlock();
        return function;
    }

    std::vector<Statement> parseBlock() {
        std::vector<Statement> body;
        if (check(TokenType::RIGHT_BRACE)) { advance(); return body; }
        consumeBlockStart("본문의 여는 중괄호 뒤에는 줄바꿈이 필요합니다");
        skipNewlines();
        while (!check(TokenType::RIGHT_BRACE)) {
            if (isAtEnd()) throw error("본문을 닫는 '}'가 필요합니다");
            body.push_back(parseStatement());
            consumeBlockMemberEnd("문장 뒤에는 줄바꿈이 필요합니다");
            skipNewlines();
        }
        advance();
        return body;
    }

    Statement parseStatement() {
        ExpressionGuard guard(statementDepth_);
        if (statementDepth_ > 128) throw error("문장 최대 중첩 깊이를 초과했습니다");
        if (check(TokenType::LET)) {
            const auto variable = parseVariable();
            Statement statement;
            statement.kind = Statement::Kind::VariableDeclaration;
            statement.name = variable.name;
            statement.line = variable.line;
            statement.span = variable.span;
            statement.id = variable.id;
            statement.annotation = variable.annotation;
            statement.expression = variable.initializer;
            return statement;
        }
        const auto token = advance();
        Statement statement;
        statement.line = token.line;
        statement.span = token.span;
        statement.id = nextId_++;
        switch (token.type) {
            case TokenType::CALL: {
                statement.kind = Statement::Kind::FunctionCall;
                statement.name = expect(TokenType::IDENTIFIER, "call 다음에는 함수 이름이 필요합니다").value;
                expect(TokenType::LEFT_PAREN, "호출 이름 뒤에는 '('가 필요합니다");
                statement.callArguments = parseArguments();
                for (const auto& argument : statement.callArguments) {
                    statement.arguments.push_back(argument->kind == Expression::Kind::Name ? argument->text : "");
                }
                break;
            }
            case TokenType::IDENTIFIER:
                statement.kind = Statement::Kind::Assignment;
                statement.name = token.value;
                expect(TokenType::ASSIGN, "변수 이름 뒤에는 '='가 필요합니다");
                statement.expression = parseExpression();
                break;
            case TokenType::RETURN:
                statement.kind = Statement::Kind::Return;
                if (!check(TokenType::NEWLINE) && !check(TokenType::END)) statement.expression = parseExpression();
                break;
            case TokenType::BREAK: statement.kind = Statement::Kind::Break; break;
            case TokenType::CONTINUE: statement.kind = Statement::Kind::Continue; break;
            case TokenType::FOR:
                statement.kind = Statement::Kind::For;
                expect(TokenType::LEFT_PAREN, "for 뒤에는 '('가 필요합니다");
                if (!check(TokenType::SEMICOLON)) statement.initializer.push_back(parseForClause(true));
                expect(TokenType::SEMICOLON, "for 초기화 뒤에는 ';'가 필요합니다");
                if (!check(TokenType::SEMICOLON)) statement.expression = parseExpression();
                expect(TokenType::SEMICOLON, "for 조건 뒤에는 ';'가 필요합니다");
                if (!check(TokenType::RIGHT_PAREN)) statement.update.push_back(parseForClause(false));
                expect(TokenType::RIGHT_PAREN, "for 헤더를 닫는 ')'가 필요합니다");
                expect(TokenType::LEFT_BRACE, "for 본문을 여는 '{'가 필요합니다");
                statement.body = parseBlock();
                break;
            case TokenType::LEFT_BRACE:
                statement.kind = Statement::Kind::Block;
                statement.body = parseBlock();
                break;
            case TokenType::IF:
            case TokenType::WHILE: {
                statement.kind = token.type == TokenType::IF ? Statement::Kind::If : Statement::Kind::While;
                statement.expression = parseExpression();
                expect(TokenType::LEFT_BRACE, "조건 뒤에는 '{'가 필요합니다");
                statement.body = parseBlock();
                if (token.type == TokenType::IF) {
                    std::size_t next = pos_;
                    while (tokens_[next].type == TokenType::NEWLINE) ++next;
                    if (tokens_[next].type == TokenType::ELSE) {
                        pos_ = next + 1;
                        if (check(TokenType::IF)) statement.alternative.push_back(parseStatement());
                        else {
                            expect(TokenType::LEFT_BRACE, "else 뒤에는 '{' 또는 if가 필요합니다");
                            statement.alternative = parseBlock();
                        }
                    }
                }
                break;
            }
            default: throw std::runtime_error(sourceLocation(token.span) + " 지원하지 않는 문장입니다");
        }
        statement.span.endLine = tokens_[pos_ - 1].span.endLine;
        statement.span.endColumn = tokens_[pos_ - 1].span.endColumn;
        return statement;
    }

    Statement parseForClause(bool allowDeclaration) {
        if (check(TokenType::IDENTIFIER) || check(TokenType::CALL) ||
            (allowDeclaration && check(TokenType::LET))) return parseStatement();
        throw error(allowDeclaration ? "for 초기화에는 let, 대입 또는 call이 필요합니다"
                                    : "for 증감에는 대입 또는 call이 필요합니다");
    }

    std::vector<Expr> parseArguments() {
        std::vector<Expr> arguments;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                arguments.push_back(parseExpression());
                if (!check(TokenType::COMMA)) break;
                advance();
            } while (true);
        }
        expect(TokenType::RIGHT_PAREN, "인자 목록을 닫는 ')'가 필요합니다");
        return arguments;
    }

    LayerDecl parseLayer() {
        Token kw = advance();                 // LAYER
        Token upper = expect(TokenType::IDENTIFIER,
                             "layer 다음에는 계층 이름이 와야 합니다");
        expect(TokenType::ABOVE, "계층 선언에는 'above'가 필요합니다");
        Token lower = expect(TokenType::IDENTIFIER,
                             "above 다음에는 하위 계층 이름이 와야 합니다");
        LayerDecl decl;
        decl.upper = upper.value;
        decl.lower = lower.value;
        decl.line  = kw.line;
        decl.span = kw.span;
        return decl;
    }

    ValueType parseType() {
        const auto token = expect(TokenType::IDENTIFIER, "타입 이름이 필요합니다");
        if (token.value == "int") return ValueType::Int;
        if (token.value == "bool") return ValueType::Bool;
        if (token.value == "string") return ValueType::String;
        if (token.value == "unit") return ValueType::Unit;
        throw std::runtime_error(sourceLocation(token.span) + " 알 수 없는 타입: " + token.value);
    }

    static int precedence(TokenType type) {
        switch (type) {
            case TokenType::OR: return 1;
            case TokenType::AND: return 2;
            case TokenType::EQUAL: case TokenType::NOT_EQUAL: return 3;
            case TokenType::LESS: case TokenType::LESS_EQUAL:
            case TokenType::GREATER: case TokenType::GREATER_EQUAL: return 4;
            case TokenType::PLUS: case TokenType::MINUS: return 5;
            case TokenType::STAR: case TokenType::SLASH: case TokenType::PERCENT: return 6;
            default: return 0;
        }
    }

    Expr node(Expression::Kind kind, const Token& token) {
        auto expr = std::make_shared<Expression>();
        expr->kind = kind; expr->id = nextId_++; expr->span = token.span;
        expr->text = token.value;
        return expr;
    }

    // Bounds recursive parsing, type checking, evaluation and destruction.
    struct ExpressionGuard {
        std::size_t& depth;
        explicit ExpressionGuard(std::size_t& d) : depth(d) { ++depth; }
        ~ExpressionGuard() { --depth; }
    };

    Expr parseExpression(int minimum = 1) {
        ExpressionGuard guard(expressionDepth_);
        if (expressionDepth_ > 128) throw error("표현식 최대 중첩 깊이를 초과했습니다");
        Expr left = parseUnary();
        while (precedence(peek().type) >= minimum) {
            const auto op = advance();
            auto expr = node(Expression::Kind::Binary, op);
            expr->left = left;
            expr->right = parseExpression(precedence(op.type) + 1);
            expr->treeDepth = 1 + std::max(left->treeDepth, expr->right->treeDepth);
            if (expr->treeDepth > 128) throw error("표현식 최대 중첩 깊이를 초과했습니다");
            expr->span = left->span;
            expr->span.endLine = expr->right->span.endLine;
            expr->span.endColumn = expr->right->span.endColumn;
            left = expr;
        }
        return left;
    }

    Expr integerLiteral(const Token& token, bool negative = false) {
        std::uint64_t magnitude = 0;
        const auto parsed = std::from_chars(token.value.data(), token.value.data() + token.value.size(), magnitude);
        const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (parsed.ec != std::errc{} || parsed.ptr != token.value.data() + token.value.size() || magnitude > maximum + (negative ? 1U : 0U)) {
            throw std::runtime_error(sourceLocation(token.span) + " integer_overflow: 정수 리터럴이 int64 범위를 초과했습니다");
        }
        auto expr = node(Expression::Kind::Literal, token);
        expr->literal = negative && magnitude == maximum + 1
            ? std::numeric_limits<std::int64_t>::min()
            : negative ? -static_cast<std::int64_t>(magnitude) : static_cast<std::int64_t>(magnitude);
        return expr;
    }

    Expr parseUnary() {
        ExpressionGuard guard(expressionDepth_);
        if (expressionDepth_ > 128) throw error("표현식 최대 중첩 깊이를 초과했습니다");
        if (check(TokenType::MINUS) || check(TokenType::PLUS) || check(TokenType::BANG)) {
            const auto op = advance();
            if (op.type == TokenType::MINUS && check(TokenType::INTEGER)) {
                auto expr = integerLiteral(advance(), true);
                expr->span.column = op.span.column;
                return expr;
            }
            auto expr = node(Expression::Kind::Unary, op);
            expr->right = parseUnary();
            expr->treeDepth = 1 + expr->right->treeDepth;
            if (expr->treeDepth > 128) throw error("표현식 최대 중첩 깊이를 초과했습니다");
            expr->span.endLine = expr->right->span.endLine;
            expr->span.endColumn = expr->right->span.endColumn;
            return expr;
        }
        const auto token = advance();
        if (token.type == TokenType::INTEGER) return integerLiteral(token);
        auto expr = node(Expression::Kind::Literal, token);
        if (token.type == TokenType::STRING) expr->literal = token.value;
        else if (token.type == TokenType::TRUE_VALUE || token.type == TokenType::FALSE_VALUE) expr->literal = token.type == TokenType::TRUE_VALUE;
        else if (token.type == TokenType::IDENTIFIER) {
            expr->kind = Expression::Kind::Name;
            if (check(TokenType::LEFT_PAREN)) {
                advance();
                expr->kind = Expression::Kind::Call;
                expr->arguments = parseArguments();
                for (const auto& argument : expr->arguments) expr->treeDepth = std::max(expr->treeDepth, 1 + argument->treeDepth);
                if (expr->treeDepth > 128) throw error("표현식 최대 중첩 깊이를 초과했습니다");
                expr->span.endLine = tokens_[pos_ - 1].span.endLine;
                expr->span.endColumn = tokens_[pos_ - 1].span.endColumn;
            }
        }
        else if (token.type == TokenType::LEFT_PAREN) {
            if (!check(TokenType::RIGHT_PAREN)) expr = parseExpression();
            const auto close = expect(TokenType::RIGHT_PAREN, "표현식을 닫는 ')'가 필요합니다");
            expr->span = token.span;
            expr->span.endLine = close.span.endLine;
            expr->span.endColumn = close.span.endColumn;
        } else throw std::runtime_error(sourceLocation(token.span) + " 표현식이 필요합니다");
        return expr;
    }

    // ── 토큰 유틸 ──────────────────────────────────────
    bool isAtEnd() const { return peek().type == TokenType::END; }
    const Token& peek() const { return tokens_[pos_]; }
    bool check(TokenType t) const { return peek().type == t; }

    void skipNewlines() {
        while (check(TokenType::NEWLINE)) advance();
    }

    Token advance() {
        Token t = tokens_[pos_];
        if (!isAtEnd()) pos_++;
        return t;
    }

    Token expect(TokenType t, const std::string& msg) {
        if (check(t)) return advance();
        throw error(msg);
    }

    // 선언 끝: NEWLINE 또는 파일 끝
    void consumeLineEnd() {
        if (check(TokenType::NEWLINE)) { advance(); return; }
        if (isAtEnd()) return;
        throw error("한 줄에는 하나의 선언만 올 수 있습니다");
    }

    void consumeBlockStart(const std::string& message) {
        if (!check(TokenType::NEWLINE)) throw error(message);
        advance();
    }

    void consumeBlockMemberEnd(const std::string& message) {
        if (check(TokenType::NEWLINE)) {
            advance();
            return;
        }
        throw error(message);
    }

    std::runtime_error error(const std::string& msg) const {
        return std::runtime_error(
            sourceLocation(peek().span) + " [" + std::to_string(peek().line) + "행] 파싱 오류: " + msg +
            " (현재 토큰: " + tokenTypeName(peek().type) +
            (peek().value.empty() ? "" : " '" + peek().value + "'") + ")");
    }
};

#endif // IEUM_PARSER_H
