#ifndef FLASHPOINT_DIAGNOSTICWRITER_H
#define FLASHPOINT_DIAGNOSTICWRITER_H

#include <program/graphql/graphql_syntaxes.h>
#include <lib/text_annotater.h>

using namespace flashpoint::program::graphql;

namespace flashpoint::test {
    class DiagnosticWriter {
    public:
        DiagnosticWriter(SchemaDocument* schema);
        DiagnosticWriter(SchemaDocument* schema, ExecutableDefinition* query);
        std::string to_string();
    private:
        SchemaDocument* schema;
        ExecutableDefinition* query;
    };
}


#endif //FLASHPOINT_DIAGNOSTICWRITER_H
