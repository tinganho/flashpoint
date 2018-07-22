#include "diagnostic_writer.h"

namespace flashpoint::test {

void
DiagnosticWriter::add_source(const Glib::ustring &source)
{
    annotated_source += source;
}

void
DiagnosticWriter::add_diagnostics(const std::vector<DiagnosticMessage>& diagnostics, const Glib::ustring& source) {
    TextAnnotater text_annotater(source);
    for (auto& diagnostic : diagnostics) {
        text_annotater.annotate(diagnostic.message, diagnostic.location);
    }
    annotated_source += text_annotater.to_string();
}

Glib::ustring
DiagnosticWriter::to_string()
{
    return annotated_source;
}

}