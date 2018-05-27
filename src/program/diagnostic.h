#ifndef LYA_DIAGNOSTIC_H_H
#define LYA_DIAGNOSTIC_H_H

#include <vector>
#include "utils.h"
#include "types.h"
#include <boost/regex.hpp>


using namespace flashpoint::lib;

namespace flashpoint::program {

    struct DiagnosticMessage {
        std::string message;
        Location location;
    };

    struct DiagnosticMessageTemplate {
        std::string message_template;
    };

    DiagnosticMessage create_diagnostic(const Location& location, const DiagnosticMessageTemplate& d);
    DiagnosticMessage create_diagnostic(const Location& location, const DiagnosticMessageTemplate& d, const std::string& arg1);
    DiagnosticMessage create_diagnostic(const Location& location, const DiagnosticMessageTemplate& d, const std::string& arg1, const std::string& arg2);
    DiagnosticMessage create_diagnostic(const Location& location, const DiagnosticMessageTemplate& d, const std::string& arg1, const std::string& arg2, const std::string& arg3);

    template<typename T>
    class DiagnosticTrait {
    protected:
        void add_diagnostic(const Location& location, const DiagnosticMessageTemplate& _template);
        void add_diagnostic(const Location& location, const DiagnosticMessageTemplate& _template, const std::string& arg1);
        void add_diagnostic(const Location& location, const DiagnosticMessageTemplate& _template, const std::string& arg1, const std::string& arg2);
        void add_diagnostic(const Location& location, const DiagnosticMessageTemplate& _template, const std::string& arg1, const std::string& arg2, const std::string& arg3);
        void add_diagnostic(const DiagnosticMessageTemplate& _template);
        void add_diagnostic(const DiagnosticMessageTemplate& _template, const std::string& arg1);
        void add_diagnostic(const DiagnosticMessageTemplate& _template, const std::string& arg1, const std::string& arg2);
        void add_diagnostic(const DiagnosticMessageTemplate& _template, const std::string& arg1, const std::string& arg2, const std::string& arg3);
        std::vector<DiagnosticMessage> diagnostics;
    };
} // Lya::lib

#include "diagnostic_impl.h"

#endif //LYA_DIAGNOSTIC_H_H
