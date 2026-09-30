#ifndef IEUM_AST_H
#define IEUM_AST_H

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "source.h"
#include "value.h"

struct Expression {
    enum class Kind { Literal, Name, Unary, Binary };
    Kind kind = Kind::Literal;
    NodeId id = 0;
    std::size_t treeDepth = 1;
    SourceSpan span = {};
    Value literal;
    std::string text; // name or operator
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
};
using Expr = std::shared_ptr<Expression>;

// ── 구조 선언과 F1 모듈 본문 AST ───────────────────────

struct VariableDecl {
    std::string name;
    int line;
    SourceSpan span = {};
    NodeId id = 0;
    std::optional<ValueType> annotation;
    Expr initializer;
};

struct Statement {
    enum class Kind {
        VariableDeclaration,
        FunctionCall,
        Assignment
    };

    Kind kind;
    std::string name;                  // 변수 이름 또는 호출 대상
    std::vector<std::string> arguments; // 호출이 아니면 비어 있음
    int line;
    SourceSpan span = {};
    NodeId id = 0;
    std::optional<ValueType> annotation;
    Expr expression;
};

struct FunctionDecl {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<Statement> body;
    int line;
    SourceSpan span = {};
    std::vector<ValueType> parameterTypes; // omitted types remain unit
};

// module <name> [depends <dep1>, <dep2>, ...] [moduleBody]
struct ModuleDecl {
    std::string name;
    std::vector<std::string> deps;  // depends 가 없으면 비어 있음
    bool hasBody = false;
    std::vector<VariableDecl> variables;
    std::vector<FunctionDecl> functions;
    int line;
    SourceSpan span = {};
};

// layer <upper> above <lower>
struct LayerDecl {
    std::string upper;
    std::string lower;
    int line;
    SourceSpan span = {};
};

// 한 소스 파일 전체 = 모듈 선언들 + 계층 선언들
struct Program {
    std::vector<ModuleDecl> modules;
    std::vector<LayerDecl> layers;
};

#endif // IEUM_AST_H
