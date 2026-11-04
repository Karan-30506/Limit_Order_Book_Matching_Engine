#ifndef CSV_CASES_HPP
#define CSV_CASES_HPP

#include "test_framework.hpp"
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace CsvCases {

struct Row {
    std::vector<std::string> fields;
    int line;
};

inline std::vector<std::string> parseLine(const std::string& text) {

    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    bool closed = false;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (quoted) {
            if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
                field += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
                closed = true;
            } else {
                field += c;
            }
        } else if (c == ',') {
            fields.push_back(field);
            field.clear();
            closed = false;
        } else if (c == '"' && field.empty() && !closed) {
            quoted = true;
        } else if (closed || c == '"') {
            throw std::runtime_error("invalid CSV quoting");
        } else {
            field += c;
        }
    }
    if (quoted) {
        throw std::runtime_error("unterminated CSV quote");
    }
    fields.push_back(field);
    return fields;
}

inline std::vector<Row> load() {

    const char* configured = std::getenv("LOB_TEST_CSV");
    const std::string path = configured ? configured : "tests/testcases.csv";
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open test CSV: " + path);
    }
    std::vector<Row> rows;
    std::string line;
    int number = 0;
    while (std::getline(input, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto fields = parseLine(line);
        if (number == 1) {
            if (line != "suite,case,operation,id,price,quantity,side,seed,steps,expected") {
                throw std::runtime_error("invalid CSV header");
            }
            continue;
        }
        if (fields.size() != 10 || fields[0].empty() || fields[1].empty()) {
            throw std::runtime_error("invalid CSV row at line " + std::to_string(number));
        }
        const std::set<std::string> suites = {
            "Order", "OrderQueue", "LimitNode", "AVLTree", "HashMap", "BookCombined", "Shared", "Benchmark", "Failure"
        };
        if (!suites.count(fields[0])) {
            throw std::runtime_error("unknown suite at line " + std::to_string(number));
        }
        rows.push_back({fields, number});
    }
    if (rows.empty()) {
        throw std::runtime_error("test CSV has no cases");
    }
    return rows;
}

using Handler = void (*)(TestFramework::TestSuite&);

inline bool run(TestFramework::TestSuite& suite, const std::string& name,
                const std::map<std::string, Handler>& handlers) {

    suite.runTest(name + ": CSV registry", [&]() {

        const auto rows = load();
        std::set<std::string> seen;
        for (const auto& row : rows) {
            if (row.fields[0] != name) {
                continue;
            }
            const auto handler = handlers.find(row.fields[1]);
            if (row.fields[2] != "unit" || handler == handlers.end() || !seen.insert(row.fields[1]).second) {
                throw std::runtime_error("unknown or duplicate unit case at CSV line " + std::to_string(row.line));
            }
        }
        if (seen.size() != handlers.size()) {
            throw std::runtime_error("CSV must list every registered " + name + " case exactly once");
        }
        for (const auto& row : rows) {
            if (row.fields[0] == name) {
                suite.runTest(name + ": " + row.fields[1], [&]() {

                    handlers.at(row.fields[1])(suite);
                });
            }
        }
    });
    return suite.allPassed();
}

} // namespace CsvCases
#endif
