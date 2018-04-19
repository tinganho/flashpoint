#include <iostream>
#include <test/baseline_test_runner.h>

using namespace flashpoint::test;

int main(int argc, char** argv) {
    BaselineTestRunner test_runner;
    test_runner.run();
}
