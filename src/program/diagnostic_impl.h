#ifndef FLASHPOINT_DIAGNOSTIC_IMPL_H
#define FLASHPOINT_DIAGNOSTIC_IMPL_H

#include <vector>
#include "diagnostic.h"
#include <glibmm/ustring.h>

using namespace flashpoint::lib;

namespace flashpoint::program {

template<typename T>
void DiagnosticTrait<T>::AddDiagnostic(const Location &location, const DiagnosticMessageTemplate &_template) {
    static_cast<T*>(this)->diagnostics.push_back(CreateDiagnostic(location, _template));
}

template<typename T>
void DiagnosticTrait<T>::AddDiagnostic(const Location &location, const DiagnosticMessageTemplate &_template,
                                       const std::string &arg1) {
    static_cast<T*>(this)->diagnostics.push_back(CreateDiagnostic(location, _template, arg1));
}

template<typename T>
void DiagnosticTrait<T>::AddDiagnostic(const Location &location, const DiagnosticMessageTemplate &_template,
                                       const std::string &arg1, const std::string &arg2) {
    static_cast<T*>(this)->diagnostics.push_back(CreateDiagnostic(location, _template, arg1, arg2));
}

template<typename T>
void DiagnosticTrait<T>::AddDiagnostic(const Location &location, const DiagnosticMessageTemplate &_template,
                                       const std::string &arg1, const std::string &arg2, const std::string &arg3) {
    static_cast<T*>(this)->diagnostics.push_back(CreateDiagnostic(location, _template, arg1, arg2, arg3));
}

template<typename T>
void DiagnosticTrait<T>::AddDiagnostic(const DiagnosticMessageTemplate &_template) {
    static_cast<T*>(this)->diagnostics.push_back(
        CreateDiagnostic(static_cast<T *>(this)->GetTokenLocation(), _template));
}

template<typename T>
void DiagnosticTrait<T>::AddDiagnostic(const DiagnosticMessageTemplate &_template, const std::string &arg1) {
    static_cast<T*>(this)->diagnostics.push_back(
        CreateDiagnostic(static_cast<T *>(this)->GetTokenLocation(), _template, arg1));
}

template<typename T>
void DiagnosticTrait<T>::AddDiagnostic(const DiagnosticMessageTemplate &_template, const std::string &arg1,
                                       const std::string &arg2) {
    static_cast<T*>(this)->diagnostics.push_back(
        CreateDiagnostic(static_cast<T *>(this)->GetTokenLocation(), _template, arg1, arg2));
}

template<typename T>
void DiagnosticTrait<T>::AddDiagnostic(const DiagnosticMessageTemplate &_template, const std::string &arg1,
                                       const std::string &arg2, const std::string &arg3) {
    static_cast<T*>(this)->diagnostics.push_back(
        CreateDiagnostic(static_cast<T *>(this)->GetTokenLocation(), _template, arg1, arg2, arg3));
}

} // Lya::lib

#endif //FLASHPOINT_DIAGNOSTIC_IMPL_H
