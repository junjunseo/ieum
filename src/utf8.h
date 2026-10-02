#ifndef IEUM_UTF8_H
#define IEUM_UTF8_H
#include <string>
inline bool validUtf8(const std::string& value) {
    for (std::size_t i = 0; i < value.size();) {
        const auto c = static_cast<unsigned char>(value[i++]);
        if (c < 0x80) continue;
        unsigned code;
        int count;
        unsigned minimum;
        if (c >= 0xC2 && c <= 0xDF) { code = c & 0x1F; count = 1; minimum = 0x80; }
        else if (c >= 0xE0 && c <= 0xEF) { code = c & 0x0F; count = 2; minimum = 0x800; }
        else if (c >= 0xF0 && c <= 0xF4) { code = c & 7; count = 3; minimum = 0x10000; }
        else return false;
        while (count--) {
            if (i == value.size()) return false;
            const auto next = static_cast<unsigned char>(value[i++]);
            if ((next & 0xC0) != 0x80) return false;
            code = (code << 6) | (next & 0x3F);
        }
        if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
    }
    return true;
}

#endif
