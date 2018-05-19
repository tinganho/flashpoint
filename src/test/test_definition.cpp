

#include <exception>
#include <string>
#include <future>
#include <iostream>
#include <chrono>
#include <vector>
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

    std::string Backtrace(int skip = 1)
    {
        void *callstack[128];
        const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
        char buf[1024];
        int nFrames = backtrace(callstack, nMaxFrames);
        char **symbols = backtrace_symbols(callstack, nFrames);

        std::ostringstream trace_buf;
        for (int i = skip; i < nFrames; i++) {
            printf("%s\n", symbols[i]);

            Dl_info info;
            if (dladdr(callstack[i], &info) && info.dli_sname) {
                char *demangled = NULL;
                int status = -1;
                if (info.dli_sname[0] == '_')
                    demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
                snprintf(buf, sizeof(buf), "%-3d %*p %s + %zd\n",
                         i, int(2 + sizeof(void*) * 2), callstack[i],
                         status == 0 ? demangled :
                         info.dli_sname == 0 ? symbols[i] : info.dli_sname,
                         (char *)callstack[i] - (char *)info.dli_saddr);
                free(demangled);
            } else {
                snprintf(buf, sizeof(buf), "%-3d %*p %s\n",
                         i, int(2 + sizeof(void*) * 2), callstack[i], symbols[i]);
            }
            trace_buf << buf;
        }
        free(symbols);
        if (nFrames == nMaxFrames)
            trace_buf << "[truncated]\n";
        return trace_buf.str();
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
            for (auto const & test : d->tests) {
                try {
                    if (test->procedure_type == ProcedureType::Normal) {
                        (test->procedure)(test);
                    }
                    else if (test->procedure_type == ProcedureType::Done) {
                        bool is_done = false;
                        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                        std::chrono::steady_clock::time_point end;
                        std::async(test->procedure_with_done, test, [&]() {
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
                        std::vector<std::string> errors;
                        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                        std::chrono::steady_clock::time_point end;
                        (test->procedure_with_done_and_error)(test, [&]() {
                            is_done = true;
                        }, [&](const std::string& error) {
                            errors.push_back(error);
                        });
                        long long int duration;
                        while (!is_done) {
                            end = std::chrono::steady_clock::now();
                            duration = std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
                            if (duration > 10) {
                                throw std::logic_error("Spent more than 5s on test.");
                            }
                        }
                        if (errors.size() > 0) {
                            throw BaselineAssertionError(join_vector(errors, "\n\n"));
                        }
                    }
                    std::cout << "\e[32m․\e[0m";
                    test->success = true;
                }
                catch (BaselineAssertionError& e) {
                    std::cout << "\e[31m․\e[0m";
                    test->success = false;
                    test->message = e.what();
                }
            }
            std::cout << std::endl;

        }
        std::cout << std::endl;
    }

} // flash::lib

