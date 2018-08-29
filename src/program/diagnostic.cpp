#include "diagnostic.h"

namespace flashpoint::program {

DiagnosticMessage
CreateDiagnostic(const Location &location, const DiagnosticMessageTemplate &d) {
    std::string message = d.message_template;
    return DiagnosticMessage { d.message_template, message, location };
}

DiagnosticMessage
CreateDiagnostic(const Location &location, const DiagnosticMessageTemplate &d, const std::string &arg1) {
    std::string message = boost::regex_replace(d.message_template, boost::regex("\\{0\\}"), arg1);
    return DiagnosticMessage { d.message_template, message, location };
}

DiagnosticMessage
CreateDiagnostic(const Location &location, const DiagnosticMessageTemplate &d, const std::string &arg1,
                 const std::string &arg2) {
    std::string message1 = boost::regex_replace(d.message_template, boost::regex("\\{0\\}"), arg1);
    std::string message2 = boost::regex_replace(message1, boost::regex("\\{1\\}"), arg2);
    return DiagnosticMessage { d.message_template, message2, location };
}

DiagnosticMessage
CreateDiagnostic(const Location &location, const DiagnosticMessageTemplate &d, const std::string &arg1,
                 const std::string &arg2, const std::string &arg3) {
    std::string message1 = boost::regex_replace(d.message_template, boost::regex("\\{0\\}"), arg1);
    std::string message2 = boost::regex_replace(message1, boost::regex("\\{1\\}"), arg2);
    std::string message3 = boost::regex_replace(message2, boost::regex("\\{2\\}"), arg3);
    return DiagnosticMessage { d.message_template, message3, location };
}

}