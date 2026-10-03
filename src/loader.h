#pragma once
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "lexer.h"
#include "parser.h"

class SourceLoader {
    using Path = std::filesystem::path;
    std::vector<Path> loaded;
    NodeId nextId = 1;
    static Path canonical(const std::string& name, const SourceSpan& at) {
        std::error_code error;
        auto path = std::filesystem::canonical(std::filesystem::u8path(name), error);
        if (error) throw SourceError(at, "path_error: 경로를 찾을 수 없습니다: " + name);
        return path;
    }
    static bool samePath(const Path& a, const Path& b) {
        std::error_code error;
        return a == b || std::filesystem::equivalent(a, b, error);
    }
    static bool hasModule(const Program& program, const std::string& name) {
        for (const auto& module : program.modules) if (module.name == name) return true;
        return false;
    }
    void append(Program& program, const Path& path, const std::string& display, const std::string& text) {
        for (const auto& previous : loaded) if (samePath(previous, path)) {
            throw SourceError({display}, "duplicate_path: 같은 소스 파일을 다시 로딩할 수 없습니다");
        }
        loaded.push_back(path);
        sources.emplace(display, text);
        Parser parser(Lexer(text, display).tokenize(), nextId);
        auto part = parser.parse(false);
        nextId = parser.nextNodeId();
        program.modules.insert(program.modules.end(), std::make_move_iterator(part.modules.begin()), std::make_move_iterator(part.modules.end()));
        program.layers.insert(program.layers.end(), std::make_move_iterator(part.layers.begin()), std::make_move_iterator(part.layers.end()));
    }
public:
    std::map<std::string, std::string> sources;
    Program load(const std::string& entry, const std::string& text, const std::vector<std::string>& searchPaths) {
        sources.clear(); loaded.clear(); nextId = 1;
        sources.emplace(entry, text);
        std::vector<Path> roots;
        for (const auto& name : searchPaths) {
            const auto root = canonical(name, {entry});
            if (!std::filesystem::is_directory(root)) throw SourceError({entry}, "path_error: 검색 경로는 디렉터리여야 합니다: " + name);
            for (const auto& previous : roots) if (samePath(previous, root)) throw SourceError({entry}, "duplicate_path: 중복 검색 경로: " + name);
            roots.push_back(root);
        }
        std::sort(roots.begin(), roots.end(), [](const auto& a, const auto& b) { return a.generic_u8string() < b.generic_u8string(); });
        Program program;
        append(program, canonical(entry, {entry}), entry, text);
        for (std::size_t i = 0; i < program.modules.size(); ++i) {
            // append may reallocate the module vector, so copy dependencies and location.
            const auto dependencies = program.modules[i].deps;
            const auto at = program.modules[i].span;
            for (const auto& name : dependencies) {
                if (isStandardModule(name) || hasModule(program, name) || roots.empty()) continue;
                std::vector<Path> candidates;
                for (const auto& root : roots) {
                    const auto file = root / std::filesystem::u8path(name + ".ieum");
                    std::error_code error;
                    const auto exists = std::filesystem::exists(file, error);
                    if (error) throw SourceError(at, "path_error: 모듈 후보 경로를 확인할 수 없습니다: " + file.u8string());
                    if (!exists) continue;
                    const auto candidate = canonical(file.u8string(), at);
                    for (const auto& previous : candidates) if (samePath(previous, candidate)) throw SourceError(at, "duplicate_path: 같은 모듈 파일의 중복 경로: " + name);
                    candidates.push_back(candidate);
                }
                if (candidates.empty()) throw SourceError(at, "module_not_found: 검색 경로에 모듈 파일이 없습니다: " + name + ".ieum");
                if (candidates.size() > 1) {
                    std::string message = "ambiguous_module_file: 여러 검색 경로에 모듈 파일이 있습니다: " + name;
                    for (const auto& candidate : candidates) message += "\n  " + candidate.u8string();
                    throw SourceError(at, message);
                }
                const auto& file = candidates.front();
                std::ifstream input(file, std::ios::binary);
                if (!input) throw SourceError(at, "source_read_error: 모듈 파일을 열 수 없습니다: " + file.u8string());
                std::string content;
                char buffer[8192];
                while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) content.append(buffer, static_cast<std::size_t>(input.gcount()));
                if (input.bad()) throw SourceError(at, "source_read_error: 모듈 파일 읽기에 실패했습니다: " + file.u8string());
                append(program, file, file.u8string(), content);
                if (!hasModule(program, name)) throw SourceError(at, "module_file_mismatch: 파일이 요청한 모듈을 선언하지 않습니다: " + name + " / " + file.u8string());
            }
        }
        installStandardLibrary(program);
        program.sources = sources;
        return program;
    }
};
