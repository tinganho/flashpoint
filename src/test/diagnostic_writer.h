#ifndef FLASHPOINT_DIAGNOSTICWRITER_H
#define FLASHPOINT_DIAGNOSTICWRITER_H

#include <program/graphql/graphql_schema.h>
#include <lib/text_annotater.h>

using namespace flashpoint::program::graphql;

namespace flashpoint::test {

class DiagnosticWriter {
public:

    void
    add_source(const Glib::ustring& source);

    void
    add_diagnostics(const std::vector<DiagnosticMessage>& diagnostics, const Glib::ustring& source);

    Glib::ustring
    to_string();

private:

    Glib::ustring
    annotated_source;

};

}


#endif //FLASHPOINT_DIAGNOSTICWRITER_H
