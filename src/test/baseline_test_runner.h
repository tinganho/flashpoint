#ifndef FLASH_BASELINE_TEST_RUNNER_H
#define FLASH_BASELINE_TEST_RUNNER_H

#include <string>
#include <functional>

namespace flashpoint::test {
    class BaselineTestRunner {
    public:
        BaselineTestRunner();
        void run();
    private:
        void visit_tests(std::function<void(const std::string&)> callback);
    };
}

#endif //FLASH_BASELINE_TEST_RUNNER_H
