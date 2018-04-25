

#include <exception>
#include <string>
#include <future>
#include <iostream>
#include <chrono>
#include <vector>
#include "utils.h"
#include "test_definition.h"

namespace flashpoint::test {

    Test::Test(const std::string& name, std::function<void(Test* t, std::function<void()>, std::function<void(std::string error)>)> procedure_with_success_and_error):
        name(name),
        procedure_type(ProcedureType::SuccessError),
        procedure_with_success_and_error(procedure_with_success_and_error) { }

    Test::Test(const std::string& name, std::function<void(Test* t, std::function<void()>)> procedure_with_done):
        name(name),
        procedure_type(ProcedureType::Done),
        procedure_with_done(procedure_with_done) { }

    Test::Test(const std::string& name, std::function<void(Test* t)> procedure):
        name(name),
        procedure_type(ProcedureType::Normal),
        procedure(procedure) { }

    Domain::Domain(std::string name):
        name(name) { }

    std::vector<Domain*> domains = {};
    Domain * current_domain;

    void domain(const std::string& name) {
        current_domain = new Domain(name);
        domains.push_back(current_domain);
    }


    void test(const std::string& name, std::function<void(Test* t, std::function<void()>, std::function<void(std::string error)>)> procedure) {
        auto test = new Test(name, procedure);
        current_domain->tests.push_back(test);
    }


    void test(const std::string& name, std::function<void(Test* t, std::function<void()>)> procedure) {
        auto test = new Test(name, procedure);
        current_domain->tests.push_back(test);
    }

    void test(const std::string& name, std::function<void(Test* t)> procedure) {
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
            std::cout << "Failed test:" << std::endl;
            std::cout << std::endl;
            for (auto const & t : failed_tests) {
                std::cout <<  "\e[31m    " + t->name + "\e[0m" << std::endl;
                std::cout <<  "\e[31m        " + t->message + "\e[0m" << std::endl;
            }
        }

        return tests_failed == 0 ? 0 : 1;
    }

    void run_test_suites() {
        std::cout << std::endl;
        for (auto const & d : domains) {
            std::cout << d->name + ":" << std::endl;
            std::cout << "    ";
            for (auto const & t : d->tests) {
                try {
                    if (t->procedure_type == ProcedureType::Normal) {
                        (t->procedure)(t);
                    }
                    else if (t->procedure_type == ProcedureType::Done) {
                        bool is_done = false;
                        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                        std::chrono::steady_clock::time_point end;
                        std::async(t->procedure_with_done, t, [&]() {
                            is_done = true;
                        });
                        long long int duration;
                        while (!is_done) {
                            end = std::chrono::steady_clock::now();
                            duration = std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
                            if (duration > 10) {
                                throw std::logic_error("Spent more than 5s on test.");
                            }
                        }
                    }
                    else {
                        bool is_done = false;
                        std::string error("");
                        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                        std::chrono::steady_clock::time_point end;
                        (t->procedure_with_success_and_error)(t, [&]() {
                            is_done = true;
                            std::cout << "hello" << std::endl;
                        }, [&](std::string err) {
                            is_done = true;
                            error = err;
                        });
                        long long int duration;
                        while (!is_done) {
                            end = std::chrono::steady_clock::now();
                            duration = std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
                            if (duration > 10) {
                                throw std::logic_error("Spent more than 5s on test.");
                            }
                        }
                        if (error != "") {
                            throw std::logic_error(error);
                        }
                    }
                    std::cout << "\e[32m․\e[0m";
                    t->success = true;
                }
                catch (std::exception& e) {
                    std::cout << "\e[31m․\e[0m";
                    t->success = false;
                    t->message = e.what();
                }
            }
            std::cout << std::endl;

        }
        std::cout << std::endl;
    }

} // flash::lib

