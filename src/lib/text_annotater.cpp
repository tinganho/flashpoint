
#include <sstream>
#include <string>
#include <functional>
#include "text_annotater.h"
#include "text_writer.h"
#include "utils.h"


namespace flashpoint::lib {

	TextAnnotater::TextAnnotater(const std::string& source) {
		set_source(source);
	}

	void TextAnnotater::set_source(const std::string &source)
    {
        lines.clear();
        std::stringstream ss;
        ss.str(source);
        std::string item;
        while (getline(ss, item)) {
            lines.push_back(item);
        }
        if (item == "") {
            lines.push_back("");
        }
        annotations.clear();
    }

	void TextAnnotater::annotate(const std::string& text, Location& location) {
        if (location.end_of_source) {
            location.line = lines.size();
        }
		auto it = annotations.find(location.line);
		Annotation annotation { location, text };
		if (it != annotations.end()) {
			auto& vec = it->second;
			auto last_annotation = vec.back().location;
			vec.push_back(annotation);
		}
		else {
			std::vector<Annotation> a = { annotation };
			annotations[location.line] = a;
		}
	}

	std::string TextAnnotater::to_string() {
		TextWriter writer;
		for (std::size_t i = 0; i < lines.size(); i++) {
			writer.write(lines[i]);
			writer.save();
			writer.newline();
			std::size_t size = lines[i].size();
			auto it = annotations.find(i + 1);

			if (it != annotations.end()) {
				std::vector<Annotation> annotations = it->second;
                std::sort(annotations.begin(), annotations.end(), [](const Annotation& a, const Annotation& b) {
                    if (a.location.end_of_source) {
                        return false;
                    }
                    if (b.location.end_of_source) {
                        return true;
                    }
                    if (a.location.line < b.location.line) {
                        return true;
                    }
                    else if (a.location.column < b.location.column) {
                        return true;
                    }
                    return false;
                });
                std::vector<Annotation> _annotations;
                std::transform(annotations.begin(), annotations.end(), std::back_inserter(_annotations), [=](Annotation annotation) {
                    if (annotation.location.end_of_source) {
                        annotation.location.line = lines.size();
                    }
                    return annotation;
                });
				for (const auto & annotation : _annotations) {
					auto location = annotation.location;
					for (int i1 = 1; i1 < location.column; i1++) {
						writer.write(" ");
					}
					auto max = std::min((std::size_t)location.length, size + 1 - (std::size_t)location.column);
					for (int i2 = 0; i2 < max; i2++) {
						writer.write("~");
					}
					if (!location.end_of_source) {
                        writer.write(" (" + std::to_string(location.line) + ", " + std::to_string(location.column) + ", " + std::to_string(location.length) + ")");
					}
					else {
                        writer.newline();
                        writer.write("END_OF_DOCUMENT:");
					}
					writer.newline();
					writer.write("!!! " + annotation.text);
					writer.save();
					writer.newline();
				}
			}
		}
		writer.restore();
		return *writer.text;
	}
};
