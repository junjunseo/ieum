#include "cli_args.h"

#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

// Keep paths and diagnostic source names UTF-8 on every platform.
std::vector<std::string> utf8Arguments(int argc, char** argv) {
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int count = 0;
    struct Arguments {
        LPWSTR* data;
        ~Arguments() { if (data) LocalFree(data); }
    } args{CommandLineToArgvW(GetCommandLineW(), &count)};
    if (!args.data) throw std::runtime_error("cannot read command-line arguments");
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            args.data[i], -1, nullptr, 0, nullptr, nullptr);
        if (!bytes) throw std::runtime_error("invalid Unicode command-line argument");
        std::string text(static_cast<std::size_t>(bytes), '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                args.data[i], -1, text.data(), bytes, nullptr, nullptr)) {
            throw std::runtime_error("cannot encode command-line argument");
        }
        text.pop_back();
        result.push_back(std::move(text));
    }
    return result;
#else
    return std::vector<std::string>(argv, argv + argc);
#endif
}
