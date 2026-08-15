// A deliberately tiny test harness.
//
// The point of these tests is to let the editor core be developed without a
// flash cycle per change, so the harness stays dependency-free: no gtest, no
// package manager, just a Makefile and a compiler.
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <cstdio>
#include <string>

namespace testing {

inline int &checks_run() { static int value = 0; return value; }
inline int &checks_failed() { static int value = 0; return value; }
inline const char *&current_test() { static const char *value = "<none>"; return value; }

inline void report_failure(const char *file, int line, const std::string &detail) {
    checks_failed()++;
    std::printf("  FAIL %s:%d\n       in %s\n       %s\n",
                file, line, current_test(), detail.c_str());
}

inline std::string describe(const std::string &value) { return "\"" + value + "\""; }
inline std::string describe(const char *value) { return describe(std::string(value)); }
inline std::string describe(int value) { return std::to_string(value); }
inline std::string describe(long value) { return std::to_string(value); }
inline std::string describe(unsigned long value) { return std::to_string(value); }
inline std::string describe(bool value) { return value ? "true" : "false"; }
inline std::string describe(char value) { return "'" + std::string(1, value) + "'"; }

template <typename Actual, typename Expected>
void check_equal(const char *file, int line, const char *expression,
                 const Actual &actual, const Expected &expected) {
    checks_run()++;
    if (actual == expected) { return; }
    report_failure(file, line,
                   std::string(expression) + "\n       expected " + describe(expected) +
                       " but got " + describe(actual));
}

inline void check_true(const char *file, int line, const char *expression, bool value) {
    checks_run()++;
    if (value) { return; }
    report_failure(file, line, std::string(expression) + " was false");
}

inline int summarize() {
    std::printf("\n%d checks, %d failed\n", checks_run(), checks_failed());
    return checks_failed() == 0 ? 0 : 1;
}

} // namespace testing

#define CHECK_EQ(actual, expected) \
    testing::check_equal(__FILE__, __LINE__, #actual, (actual), (expected))
#define CHECK(expression) \
    testing::check_true(__FILE__, __LINE__, #expression, (expression))

#define TEST_CASE(name)                        \
    testing::current_test() = name;            \
    std::printf("- %s\n", name);

#endif // TEST_UTIL_H
