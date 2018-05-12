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
        SpanLocation location;
    };

    struct DiagnosticMessageTemplate {
        std::string message_template;
    };

    DiagnosticMessage create_diagnostic(SpanLocation location, DiagnosticMessageTemplate& d);
    DiagnosticMessage create_diagnostic(SpanLocation location, DiagnosticMessageTemplate& d, const std::string& arg1);
    DiagnosticMessage create_diagnostic(SpanLocation location, DiagnosticMessageTemplate& d, const std::string& arg1, const std::string& arg2);

    template<class T>
    class DiagnosticTrait {
    public:
        void add_diagnostic(DiagnosticMessageTemplate _template);
        void add_diagnostic(DiagnosticMessageTemplate _template, std::string arg1);
        void add_diagnostic(DiagnosticMessageTemplate _template, std::string arg1, std::string arg2);
    protected:
        std::vector<DiagnosticMessage> diagnostics;
        bool has_diagnostics;
    };



    template<class T>
    void DiagnosticTrait<T>::add_diagnostic(DiagnosticMessageTemplate _template) {
        has_diagnostics = true;
        diagnostics.push_back(create_diagnostic(static_cast<T*>(this)->get_token_location(), _template));
    }

    template<class T>
    void DiagnosticTrait<T>::add_diagnostic(DiagnosticMessageTemplate _template, std::string arg1) {
        has_diagnostics = true;
        diagnostics.push_back(create_diagnostic(static_cast<T*>(this)->get_token_location(), _template, arg1));
    }

    template<class T>
    void DiagnosticTrait<T>::add_diagnostic(DiagnosticMessageTemplate _template, std::string arg1, std::string arg2) {
        has_diagnostics = true;
        diagnostics.push_back(create_diagnostic(static_cast<T*>(this)->get_token_location(), _template, arg1, arg2));
    }
} // Lya::lib

#endif //LYA_DIAGNOSTIC_H_H
