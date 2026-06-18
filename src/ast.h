#ifndef IEUM_AST_H
#define IEUM_AST_H

#include <string>
#include <vector>

// ── 구조 선언 AST ──────────────────────────────────────
// 1학기엔 "구조"만 표현한다. 표현식/문장 노드는 2학기에 추가.

// module <name> [depends <dep1>, <dep2>, ...]
struct ModuleDecl {
    std::string name;
    std::vector<std::string> deps;  // depends 가 없으면 비어 있음
    int line;
};

// layer <upper> above <lower>
struct LayerDecl {
    std::string upper;
    std::string lower;
    int line;
};

// 한 소스 파일 전체 = 모듈 선언들 + 계층 선언들
struct Program {
    std::vector<ModuleDecl> modules;
    std::vector<LayerDecl> layers;
};

#endif // IEUM_AST_H