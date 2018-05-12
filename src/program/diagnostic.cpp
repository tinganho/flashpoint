//
// Created by Tingan Ho on 2018-05-10.
//

#include "diagnostic.h"

namespace flashpoint::program {
    DiagnosticMessage create_diagnostic(SpanLocation location, DiagnosticMessageTemplate& d) {
        std::string message = d.message_template;
        return DiagnosticMessage { message, location };
    }

    DiagnosticMessage create_diagnostic(SpanLocation location, DiagnosticMessageTemplate& d, const std::string& arg1) {
        std::string message = boost::regex_replace(d.message_template, boost::regex("\\{0\\}"), arg1);
        return DiagnosticMessage { message, location };
    }

    DiagnosticMessage create_diagnostic(SpanLocation location, DiagnosticMessageTemplate& d, const std::string& arg1, const std::string& arg2) {
        std::string message1 = boost::regex_replace(d.message_template, boost::regex("\\{0\\}"), arg1);
        std::string message2 = boost::regex_replace(message1, boost::regex("\\{1\\}"), arg2);
        return DiagnosticMessage { message, location };
    }
}