#ifndef TEST_FRAMEWORK_HPP
#define TEST_FRAMEWORK_HPP

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <exception>

namespace TestFramework {

struct TestFailure {
    std::string testName;
    std::string file;
    int line;
    std::string message;
};

class TestSuite {
public:
    std::string suiteName;
    int testsRun = 0;
    int testsPassed = 0;
    std::vector<TestFailure> failures;

    explicit TestSuite(std::string name) : suiteName(std::move(name)) {}

    void reportAssertionFailure(const std::string& currentTest, const char* file, int line, const std::string& message) {
        failures.push_back({currentTest, file, line, message});
    }

    void runTest(const std::string& name, const std::function<void()>& testFunc) {
        ++testsRun;
        size_t initialFailures = failures.size();
        try {
            testFunc();
        } catch (const std::exception& e) {
            reportAssertionFailure(name, "unknown", 0, std::string("Unexpected exception thrown: ") + e.what());
        } catch (...) {
            reportAssertionFailure(name, "unknown", 0, "Unknown exception thrown");
        }

        if (failures.size() == initialFailures) {
            ++testsPassed;
            std::cout << "  [PASS] " << name << "\n";
        } else {
            std::cout << "  [FAIL] " << name << "\n";
            for (size_t i = initialFailures; i < failures.size(); ++i) {
                std::cout << "         " << failures[i].file << ":" << failures[i].line 
                          << " -> " << failures[i].message << "\n";
            }
        }
    }

    void printSummary() const {
        std::cout << "--------------------------------------------------\n";
        std::cout << "Suite: " << suiteName << " Summary: "
                  << testsPassed << "/" << testsRun << " passed";
        if (failures.empty()) {
            std::cout << " (ALL PASSED)\n";
        } else {
            std::cout << " (" << failures.size() << " FAILED)\n";
        }
        std::cout << "--------------------------------------------------\n";
    }

    bool allPassed() const {
        return failures.empty();
    }
};

} // namespace TestFramework

#define ASSERT_TRUE_MSG(expr, msg) \
    do { \
        if (!(expr)) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_TRUE failed (" #expr "): " << msg; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
    } while (0)

#define ASSERT_TRUE(expr) ASSERT_TRUE_MSG(expr, "")

#define ASSERT_FALSE_MSG(expr, msg) \
    do { \
        if (expr) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_FALSE failed (" #expr "): " << msg; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_FALSE_MSG(expr, "")

#define ASSERT_EQ(actual, expected) \
    do { \
        auto _act = (actual); \
        auto _exp = (expected); \
        if (!(_act == _exp)) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_EQ failed: " #actual " (" << _act << ") != " #expected " (" << _exp << ")"; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
    } while (0)

#define ASSERT_NE(actual, expected) \
    do { \
        auto _act = (actual); \
        auto _exp = (expected); \
        if (_act == _exp) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_NE failed: " #actual " == " #expected " (" << _act << ")"; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
    } while (0)

#define ASSERT_NULL(expr) \
    do { \
        if ((expr) != nullptr) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_NULL failed: " #expr " is not null (" << (void*)(expr) << ")"; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(expr) \
    do { \
        if ((expr) == nullptr) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_NOT_NULL failed: " #expr " is null"; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
    } while (0)

#define ASSERT_THROWS(expr, ExceptionType) \
    do { \
        bool _threw = false; \
        try { \
            (expr); \
        } catch (const ExceptionType&) { \
            _threw = true; \
        } catch (const std::exception& _e) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_THROWS(" #expr ", " #ExceptionType ") failed: caught different exception: " << _e.what(); \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } catch (...) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_THROWS(" #expr ", " #ExceptionType ") failed: caught non-std exception"; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
        if (!_threw) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_THROWS(" #expr ", " #ExceptionType ") failed: no exception thrown"; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
    } while (0)

#define ASSERT_NO_THROW(expr) \
    do { \
        try { \
            (expr); \
        } catch (const std::exception& _e) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_NO_THROW(" #expr ") failed: threw exception: " << _e.what(); \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } catch (...) { \
            std::ostringstream _oss; \
            _oss << "ASSERT_NO_THROW(" #expr ") failed: threw non-std exception"; \
            suite.reportAssertionFailure(__func__, __FILE__, __LINE__, _oss.str()); \
            return; \
        } \
    } while (0)

#endif // TEST_FRAMEWORK_HPP

