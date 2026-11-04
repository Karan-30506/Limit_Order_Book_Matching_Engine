#ifndef WORKLET3_SCENARIO_RUNNER_HPP
#define WORKLET3_SCENARIO_RUNNER_HPP

#include <string>
#include <vector>

struct ScenarioResult {
    bool passed = true;
    std::size_t steps = 0;
    std::vector<std::string> errors;
};

ScenarioResult runScenarioJsonText(const std::string& text);
ScenarioResult runScenarioJsonFile(const std::string& path);

#endif
