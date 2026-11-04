#include "mini_test.hpp"

// Usage: worklet3_harness [name-filter], e.g. worklet3_harness Validator
int main(int argc, char** argv) {
    std::printf("Worklet 3 integration harness\n\n");
    return mt::runAll(argc > 1 ? argv[1] : "");
}
