// StubControl.hpp - knobs and call-recorders shared between stub implementations
// (linked in place of the real .cpp files) and the stub tests.
#ifndef STUB_CONTROL_HPP
#define STUB_CONTROL_HPP

#include <string>
#include <vector>

struct StubMatcherControl {
    // OrderValidator stub
    bool validatorResult = true;
    int validatorCalls = 0;

    // Limit / Market matcher stubs: record what MatchingEngine routed to them.
    bool matcherResult = true;
    std::vector<std::string> calls;   // e.g. "limitBuy:7", "marketSell:9"

    void reset() { *this = StubMatcherControl(); }
};
extern StubMatcherControl gStubMatchers;

struct StubLoggerControl {
    bool failOnNextLog = false;       // throw from the next log*() call
    int constructed = 0;
    void reset() { failOnNextLog = false; }
};
extern StubLoggerControl gStubLogger;

#endif
