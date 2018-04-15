


#ifndef FLASH_TEST_DEFINITION_H
#define FLASH_TEST_DEFINITION_H


#include <exception>
#include <string>
#include <iostream>
#include <vector>
#include <functional>
#include "utils.h"

namespace flash::lib {

    struct Test {
        std::string name;
        std::function<void(Test* t)> procedure;
        bool success;

        Test(std::string name, std::function<void(Test* t)> procedure);
    };

    struct Domain {
        std::string name;
        std::vector<Test*>tests = {};

        Domain(std::string name);
    };

    void domain(std::string name);
    void test(std::string name, std::function<void(Test* t)> procedure);
    int print_result();
    void run_tests();

} // Lya::TestFramework

#endif