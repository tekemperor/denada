// Host-build shim for Arduino.h.
//
// The editor core (gap buffer, text buffer, window, document, editor) is plain
// C++ and has no business depending on the Arduino runtime. It reaches for
// Serial only for debug tracing, so the host test build satisfies that with a
// sink that can be inspected when a test cares and ignored when it doesn't.
#ifndef HOST_SHIM_ARDUINO_H
#define HOST_SHIM_ARDUINO_H

#include <cstdarg>
#include <cstdio>
#include <string>

class HostSerial {
public:
    std::string output;
    bool echo_to_stdout = false;

    void begin(unsigned long) {}

    void printf(const char *format, ...) {
        char scratch[512];
        va_list args;
        va_start(args, format);
        vsnprintf(scratch, sizeof(scratch), format, args);
        va_end(args);
        emit(scratch);
    }

    void print(const char *text) { emit(text); }
    void print(char character) { char pair[2] = {character, '\0'}; emit(pair); }
    void print(int value) { printf("%d", value); }
    void println(const char *text) { emit(text); emit("\n"); }
    void println() { emit("\n"); }
    void write(char character) { print(character); }
    void flush() {}

private:
    void emit(const char *text) {
        output += text;
        if (echo_to_stdout) { std::fputs(text, stdout); }
    }
};

extern HostSerial Serial;

#endif // HOST_SHIM_ARDUINO_H
