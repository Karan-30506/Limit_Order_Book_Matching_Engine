#ifndef WORKLET3_MINI_TEST_HPP
#define WORKLET3_MINI_TEST_HPP

#include <cstdio>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace mt {

struct TestState {
    int failures = 0;
};

struct TestCase {
    std::string name;
    void (*run)();
};

inline TestState& state() {
    static TestState value;
    return value;
}

inline std::vector<TestCase>& tests() {
    static std::vector<TestCase> value;
    return value;
}

inline bool registerTest(const char* name, void (*run)()) {
    tests().push_back({name, run});
    return true;
}

inline void fail(const char* file, int line, const std::string& message) {
    ++state().failures;
    std::fprintf(stderr, "%s:%d: %s\n", file, line, message.c_str());
}

template <typename Left, typename Right>
inline void checkEqual(
    const Left& left,
    const Right& right,
    const char* leftText,
    const char* rightText,
    const char* file,
    int line
) {
    if (!(left == right)) {
        fail(file, line, std::string("expected ") + leftText + " == " + rightText);
    }
}

inline int runAll(const std::string& filter = "") {
    int totalFailures = 0;
    int runCount = 0;
    for (const TestCase& test : tests()) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) {
            continue;
        }

        state().failures = 0;
        ++runCount;
        try {
            test.run();
        } catch (const std::exception& error) {
            fail(__FILE__, __LINE__, std::string("uncaught exception: ") + error.what());
        } catch (...) {
            fail(__FILE__, __LINE__, "uncaught non-standard exception");
        }

        if (state().failures == 0) {
            std::printf("[PASS] %s\n", test.name.c_str());
        } else {
            std::fprintf(stderr, "[FAIL] %s (%d failure(s))\n",
                         test.name.c_str(), state().failures);
            totalFailures += state().failures;
        }
    }

    if (runCount == 0) {
        std::fprintf(stderr, "No tests matched filter '%s'\n", filter.c_str());
        return 1;
    }
    std::printf("%d test(s), %d failure(s)\n", runCount, totalFailures);
    return totalFailures == 0 ? 0 : 1;
}

} // namespace mt

#define MT_JOIN_IMPL(left, right) left##right
#define MT_JOIN(left, right) MT_JOIN_IMPL(left, right)

#define TEST(group, name) \
    static void MT_JOIN(mt_test_, MT_JOIN(group, MT_JOIN(_, name)))(); \
    static const bool MT_JOIN(mt_registered_, MT_JOIN(group, MT_JOIN(_, name))) = \
        mt::registerTest(#group "." #name, \
            MT_JOIN(mt_test_, MT_JOIN(group, MT_JOIN(_, name)))); \
    static void MT_JOIN(mt_test_, MT_JOIN(group, MT_JOIN(_, name)))()

#define PARAM_TEST(group, name, cases, ...) \
    static void MT_JOIN(mt_test_, MT_JOIN(group, MT_JOIN(_, name)))(); \
    static const bool MT_JOIN(mt_registered_, MT_JOIN(group, MT_JOIN(_, name))) = \
        mt::registerTest(#group "." #name, \
            MT_JOIN(mt_test_, MT_JOIN(group, MT_JOIN(_, name)))); \
    static void MT_JOIN(mt_test_, MT_JOIN(group, MT_JOIN(_, name)))() { \
        for (const auto& mt_case : (cases)) { (__VA_ARGS__)(mt_case); } \
    }

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            mt::fail(__FILE__, __LINE__, "check failed: " #expression); \
        } \
    } while (false)

#define CHECK_EQ(left, right) \
    do { \
        const auto& mt_left = (left); \
        const auto& mt_right = (right); \
        mt::checkEqual(mt_left, mt_right, #left, #right, __FILE__, __LINE__); \
    } while (false)

#endif
