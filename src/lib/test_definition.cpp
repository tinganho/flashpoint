

#include <exception>
#include <string>
#include <iostream>
#include <vector>
#include "utils.h"
#include "test_definition.h"

namespace flash::lib {

    Test::Test(std::string name, std::function<void(Test* t)> procedure):
            name(name),
            procedure(procedure) { }

    Domain::Domain(std::string name):
            name(name) { }

    std::vector<Domain*> domains = {};
    Domain * current_domain;

    void domain(std::string name) {
        current_domain = new Domain(name);
        domains.push_back(current_domain);
    }

    void test(std::string name, std::function<void(Test* t)> procedure) {
        auto test = new Test(name, procedure);
        current_domain->tests.push_back(test);
    }

    int print_result() {
        std::vector<Test*> failed_tests = {};
        int tests_succeded = 0;
        int tests_failed = 0;
        int tests = 0;
        for (auto const & d : domains) {
            for (auto const & t : d->tests) {
                if (!t->success) {
                    failed_tests.push_back(t);
                    tests_failed++;
                }
                else {
                    tests_succeded++;
                }
                tests++;
            }
        }

        std::cout << "\e[32m    " + std::to_string(tests_succeded) + " passed\e[0m" << std::endl;
        std::cout << "\e[31m    " + std::to_string(tests_failed) + " failed\e[0m" << std::endl;
        std::cout << "    " + std::to_string(tests_succeded + tests_failed) + " total" << std::endl;
        int domain_size = domains.size();
        std::string domain = domain_size == 1 ? " domain" : " domains";
        std::cout << "    " + std::to_string(domain_size) + domain << std::endl;

        if (tests_failed > 0) {
            std::cout << std::endl;
            std::cout << "Failed tests:" << std::endl;
            std::cout << std::endl;
            for (auto const & t : failed_tests) {
                std::cout <<  "\e[31m    " + t->name + "\e[0m" << std::endl;
            }
        }

        return tests_failed == 0 ? 0 : 1;
    }

    void run_tests() {
        std::cout << std::endl;
        for (auto const & d : domains) {
            std::cout << d->name + ":" << std::endl;
            std::cout << "    ";
            for (auto const & t : d->tests) {
                try {
                    t->procedure(t);
                    std::cout << "\e[32m․\e[0m";
                    t->success = true;
                }
                catch (std::exception& e) {
                    std::cout << "\e[31m․\e[0m";
                    t->success = false;
                }
            }
            std::cout << std::endl;

        }
        std::cout << std::endl;
    }

} // flash::lib

