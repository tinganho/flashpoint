#ifndef FLASH_TEST_DEFINITION_H
#define FLASH_TEST_DEFINITION_H


#include <exception>
#include <string>
#include <iostream>
#include <vector>
#include <functional>
#include "utils.h"

namespace flashpoint::test {

    enum class ProcedureType {
        Normal,
        Done,
        SuccessError,
    };

    struct Test {
        std::string name;
        ProcedureType procedure_type;
        std::function<void(Test* t, std::function<void()>, std::function<void(std::string error)>)> procedure_with_success_and_error;
        std::function<void(Test* t, std::function<void()>)> procedure_with_done;
        std::function<void(Test* t)> procedure;

        bool success;
        std::string message;
        std::function<void(std::string error)> done;

        Test(const std::string& name, std::function<void(Test* t, std::function<void()>, std::function<void(std::string error)>)>);
        Test(const std::string& name, std::function<void(Test* t, std::function<void()>)>);
        Test(const std::string& name, std::function<void(Test* t)> procedure);
    };

    struct Domain {
        std::string name;
        std::vector<Test*>tests = {};

        Domain(std::string name);
    };

    void domain(const std::string& name);
    void test(const std::string& name, std::function<void(Test* t, std::function<void()>, std::function<void(std::string error)>)> procedure);
    void test(const std::string& name, std::function<void(Test* t, std::function<void()>)> procedure);
    void test(const std::string& name, std::function<void(Test* t)> procedure);
    int print_result();
    void run_test_suites();

} // Lya::TestFramework

#endif