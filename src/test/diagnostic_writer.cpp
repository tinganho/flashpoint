#include "diagnostic_writer.h"

namespace flashpoint::test {


    DiagnosticWriter::DiagnosticWriter(SchemaDocument* schema):
        schema(schema),
        query(nullptr)
    { }

    DiagnosticWriter::DiagnosticWriter(SchemaDocument* schema, QueryDocument *query):
        schema(schema),
        query(query)
    { }

    std::string DiagnosticWriter::to_string()
    {
        TextAnnotater annotater(*schema->source);
        for (const auto diagnostic : schema->diagnostics) {
            annotater.annotate(diagnostic.message, diagnostic.location);
        }
        auto schema_source = annotater.to_string();
        if (query == nullptr) {
            return schema_source;
        }
        annotater.set_source(*query->source);
        for (const auto diagnostic : query->diagnostics) {
            annotater.annotate(diagnostic.message, diagnostic.location);
        }
        auto query_source = annotater.to_string();
        return schema_source + "====\n" + query_source;
    }
}