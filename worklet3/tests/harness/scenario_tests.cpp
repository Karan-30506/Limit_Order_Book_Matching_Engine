// Data-driven: every JSON file in the scenario data directory becomes one test.
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "ScenarioRunner.hpp"
#include "mini_test.hpp"

#ifndef LOB_DATA_DIR
#define LOB_DATA_DIR "tests/data"
#endif

namespace {
struct FileCase { std::string label; std::string path; };

std::vector<FileCase> findScenarios() {
    std::vector<FileCase> out;
    namespace fs = std::filesystem;
    if (fs::exists(LOB_DATA_DIR))
        for (auto& e : fs::directory_iterator(LOB_DATA_DIR))
            if (e.is_regular_file() && e.path().extension() == ".json")
                out.push_back({e.path().filename().string(), e.path().string()});
    std::sort(out.begin(), out.end(), [](const FileCase& a, const FileCase& b) { return a.label < b.label; });
    return out;
}
const std::vector<FileCase> kFiles = findScenarios();
} 

TEST(Scenario, DataDirectoryIsNotEmpty) {
    CHECK(!kFiles.empty());
}

PARAM_TEST(Scenario, Files, kFiles, [](const FileCase& c) {
    const ScenarioResult r = runScenarioJsonFile(c.path);
    for (const std::string& e : r.errors) mt::fail(c.path.c_str(), 0, e);
    CHECK(r.steps > 0);
});

TEST(Scenario, RunnerDetectsWrongExpectation) {
    // Meta-test: the runner itself must be able to fail.
    const ScenarioResult r = runScenarioJsonText(
        R"({"steps":[{"type":"LIMIT","side":"SELL","id":1,"price":100,"quantity":5},)"
        R"({"type":"LIMIT","side":"BUY","id":2,"price":100,"quantity":5},)"
        R"({"type":"EXPECT_TRADE","buyId":2,"sellId":1,"price":100,"quantity":99}]})");
    CHECK(!r.passed);
    CHECK_EQ((int)r.errors.size(), 1);
    CHECK(!runScenarioJsonText(R"({"steps":[{"type":"BOGUS"}]})").passed);
    const ScenarioResult invalidPrice = runScenarioJsonText(
        R"({"steps":[{"type":"EXPECT_BEST_BID","price":"not-a-price"}]})");
    CHECK(!invalidPrice.passed);
    CHECK_EQ((int)invalidPrice.errors.size(), 1);
    CHECK(!runScenarioJsonText(R"({"steps":[)").passed);
    CHECK(!runScenarioJsonText(R"({"steps":[{"type":"LIMIT","side":"BUY","id":1.5,"price":100,"quantity":1}]})").passed);
}
