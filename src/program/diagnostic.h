#ifndef LYA_DIAGNOSTIC_H_H
#define LYA_DIAGNOSTIC_H_H

#include <vector>
#include "utils.h"
#include "types.h"
#include <boost/regex.hpp>


using namespace flashpoint::lib;

namespace flashpoint::program {
    struct DiagnosticMessageTemplate {
        std::string message_template;
    };

    struct DiagnosticMessage {
        std::string _template;
        std::string message;
        Location location;
    };

    DiagnosticMessage CreateDiagnostic(
        const Location &location,
        const DiagnosticMessageTemplate &d);
    DiagnosticMessage CreateDiagnostic(
        const Location &location,
        const DiagnosticMessageTemplate &d,
        const std::string &arg1);
    DiagnosticMessage CreateDiagnostic(
        const Location &location,
        const DiagnosticMessageTemplate &d,
        const std::string &arg1,
        const std::string &arg2);
    DiagnosticMessage CreateDiagnostic(
        const Location &location,
        const DiagnosticMessageTemplate &d,
        const std::string &arg1,
        const std::string &arg2,
        const std::string &arg3);

    template<typename T>
    class DiagnosticTrait {
    public:
        std::vector<DiagnosticMessage> diagnostics;
    protected:
        void AddDiagnostic(
            const Location &location,
            const DiagnosticMessageTemplate &_template);
        void AddDiagnostic(
            const Location &location,
            const DiagnosticMessageTemplate &_template,
            const std::string &arg1);
        void AddDiagnostic(
            const Location &location,
            const DiagnosticMessageTemplate &_template,
            const std::string &arg1,
            const std::string &arg2);
        void AddDiagnostic(
            const Location &location,
            const DiagnosticMessageTemplate &_template,
            const std::string &arg1,
            const std::string &arg2,
            const std::string &arg3);
        void AddDiagnostic(
            const DiagnosticMessageTemplate &_template);
        void AddDiagnostic(
            const DiagnosticMessageTemplate &_template,
            const std::string &arg1);
        void AddDiagnostic(
            const DiagnosticMessageTemplate &_template,
            const std::string &arg1,
            const std::string &arg2);
        void AddDiagnostic(
            const DiagnosticMessageTemplate &_template,
            const std::string &arg1,
            const std::string &arg2,
            const std::string &arg3);
    };
} // Lya::lib

#include "diagnostic_impl.h"

#endif //LYA_DIAGNOSTIC_H_H
