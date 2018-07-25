#ifndef LYA_TEXTANNOTATEWRITER_H
#define LYA_TEXTANNOTATEWRITER_H

#include <string>
#include <vector>
#include <map>
#include "types.h"


namespace flashpoint::lib {

struct Annotation {
    Location
    location;

    std::string
    text;
};

class TextAnnotater {
public:
    explicit
    TextAnnotater(const std::string& source);

    void
    annotate(const std::string& source, const Location& location);

    void
    set_source(const std::string& source);

    std::string
    to_string();

private:

    typedef std::map<std::size_t, std::vector<Annotation>>
    AnnotationMap;

    std::vector<std::string>
    lines;

    AnnotationMap
    annotations;
};
}

#endif //LYA_TEXTANNOTATEWRITER_H
