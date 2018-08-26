#include <exception>
#include <string>
#include <future>
#include <iostream>
#include <chrono>
#include <vector>
#include <sys/ioctl.h>
#include <iomanip>
#include "utils.h"
#include "test_definition.h"
#include <execinfo.h> // for backtrace
#include <dlfcn.h>    // for dladdr
#include <cxxabi.h>   // for __cxa_demangle

using namespace flashpoint::lib;

namespace flashpoint::test {

    BaselineAssertionError::BaselineAssertionError(const std::string& message) throw() :
        std::runtime_error(message)
    { }

    const char* BaselineAssertionError::what() const throw() {
        return std::runtime_error::what();
    }

    Test::Test(const std::string& name, std::function<void(Test* t, std::function<void()>, std::function<void(std::string error)>)> procedure_with_success_and_error):
        name(name),
        procedure_type(ProcedureType::SuccessError),
        procedure_with_done_and_error(procedure_with_success_and_error) { }

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


    std::chrono::steady_clock::time_point start_all;
    std::chrono::steady_clock::time_point end_all;


std::string GetTimeString(long long int ms) {
    std::stringstream ss;
    if (ms < 60 * 1000) {
        ss << std::setprecision(2) << std::fixed << ms / 1000.0 << "s";
    }
    else {
        ss << std::setprecision(2) << std::fixed << ms / (60 * 1000.0) << "m";
    }
    return ss.str();
}

int PrintStats() {
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
    std::cout << "Stats:" << std::endl << std::endl;
    std::cout << "\e[32m    " + std::to_string(tests_succeded) + " passed\e[0m\n";
    std::cout << "\e[31m    " + std::to_string(tests_failed) + " failed\e[0m\n";
    std::cout << "    " + std::to_string(tests_succeded + tests_failed) + " total\n";
    int domain_size = domains.size();
    std::string domain = domain_size == 1 ? " domain" : " domains";
    std::cout << "    " + std::to_string(domain_size) + domain << std::endl << std::endl;
    std::cout << "    "  << GetTimeString(
            std::chrono::duration_cast<std::chrono::milliseconds>(end_all - start_all).count()) << " total time" << std::endl;

    if (tests_failed > 0) {
        std::cout << std::endl;
        std::cout << "Failed test:" << std::endl;
        std::cout << std::endl;
        for (auto const & t : failed_tests) {
            std::cout <<  "\e[31m    " + t->name + "\e[0m" << "\n";
            std::cout <<  "\e[31m        " + t->message + "\e[0m" << "\n";
        }
    }

    return tests_failed == 0 ? 0 : 1;
}

void RunTestSuites(bool with_timeout) {
    start_all = std::chrono::steady_clock::now();
    std::cout << std::endl;
    std::cout << "Tests:" << std::endl << std::endl;
    for (auto const & d : domains) {
        std::cout << "  " << d->name + ":" << std::endl << std::endl;
        float test_count = 0.0;
        for (auto const& test : d->tests) {
            bool failed = false;
            try {
                if (test->procedure_type == ProcedureType::Normal) {
                    (test->procedure)(test);
                }
                else if (test->procedure_type == ProcedureType::Done) {
                    bool is_done = false;
                    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
                    std::chrono::steady_clock::time_point end;
                    std::async(test->procedure_with_done, test, [&]() {
                        is_done = true;
                    });
                    long long int duration;
                    while (!is_done) {
                        end = std::chrono::steady_clock::now();
                        duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
                        if (duration > 10) {
                            throw std::logic_error("Spent more than 10s on test.");
                        }
                    }
                }
                else {
                    bool is_done = false;
                    std::vector<std::string> errors;
                    std::chrono::steady_clock::time_point begin;
                    if (with_timeout) {
                        begin = std::chrono::steady_clock::now();
                    }
                    std::chrono::steady_clock::time_point end;
                    (test->procedure_with_done_and_error)(test, [&]() {
                        is_done = true;
                    }, [&](const std::string& error) {
                        errors.push_back(error);
                    });
                    long long int duration;
                    if (with_timeout) {
                        while (!is_done) {
                            end = std::chrono::steady_clock::now();
                            duration = std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
                            if (duration > 10) {
                                throw std::logic_error("Spent more than 10s on test.");
                            }
                        }
                    }
                    if (errors.size() > 0) {
                        throw BaselineAssertionError(join_vector(errors, "\n\n"));
                    }
                }
                test->success = true;
                std::cout << "    \e[32m\u2714\e[0m";
            }
            catch (BaselineAssertionError& e) {
                test->success = false;
                test->message = e.what();
                std::cout << "    \e[31m\u2718";
            }
            test_count++;
            std::cout << " " + test->name;
            if (!test->success) {
                std::cout << "\e[0m" << std::endl;
            }
            else {
                std::cout << std::endl;
            }
        }
        std::cout << std::endl;
        std::cout << "    " << d->tests.size() << " total";
        std::cout << std::endl << std::endl;
    }
    std::cout << std::endl;
    end_all = std::chrono::steady_clock::now();
}

} // flash::lib

