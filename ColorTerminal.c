#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <stdio.h>

#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"

static int enable_terminal_colors(void) {
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    if (output == INVALID_HANDLE_VALUE || !GetConsoleMode(output, &mode)) {
        return 0;
    }

    return SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
}

int main(void) {
    enable_terminal_colors();

    printf(COLOR_BOLD COLOR_CYAN "C color terminal demo" COLOR_RESET "\n");
    printf(COLOR_RED "Red: error or removed" COLOR_RESET "\n");
    printf(COLOR_GREEN "Green: success or added" COLOR_RESET "\n");
    printf(COLOR_YELLOW "Yellow: warning or modified" COLOR_RESET "\n");
    printf(COLOR_BLUE "Blue: information" COLOR_RESET "\n");
    printf(COLOR_MAGENTA "Magenta: socket client" COLOR_RESET "\n");
    printf(COLOR_CYAN "Cyan: watcher server" COLOR_RESET "\n");

    return 0;
}