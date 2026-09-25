#ifndef TERMINAL_COLORS_HPP
#define TERMINAL_COLORS_HPP

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace terminal {

const char* const reset = "\x1b[0m";
const char* const red = "\x1b[31m";
const char* const green = "\x1b[32m";
const char* const yellow = "\x1b[33m";
const char* const blue = "\x1b[34m";
const char* const magenta = "\x1b[35m";
const char* const cyan = "\x1b[36m";
const char* const bold = "\x1b[1m";

inline void enableColors() {
#ifdef _WIN32
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &mode)) {
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

} // namespace terminal

#endif