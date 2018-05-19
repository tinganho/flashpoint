//
// Created by Tingan Ho on 2017-10-14.
//

#ifndef LYA_TEXTWRITER_H
#define LYA_TEXTWRITER_H

#include <string>
#include <vector>
#include <map>
#include <stack>

namespace flashpoint::lib {

	struct TextCursor {
		unsigned long position;
		unsigned long column;
		unsigned int indentation;

		TextCursor(
			unsigned long position,
			unsigned long column,
			unsigned int indentation):

			position(position),
			column(column),
			indentation(indentation) { }
	};

	struct TextAndTextCursor : TextCursor {
		std::unique_ptr<std::string> text;

		TextAndTextCursor(
			unsigned long position,
			unsigned long column,
			unsigned int indentation,
			std::unique_ptr<std::string>& text):

			TextCursor(position, column, indentation),
			text(std::move(text)) { }
	};

	struct PlaceholderTextCursor : TextCursor {
		std::unique_ptr<std::string> placeholder;
		std::unique_ptr<std::string> text_end;

		PlaceholderTextCursor(
			unsigned long position,
			unsigned long column,
			unsigned int indentiation,
            std::unique_ptr<std::string>& placeholder,
			std::unique_ptr<std::string>& text_end):

			TextCursor(position, column, indentiation),
			placeholder(std::move(placeholder)),
			text_end(std::move(text_end))
			{ }
	};

	class TextWriter {
	public:
		TextWriter();
		std::unique_ptr<std::string> text;
		void add_tab(unsigned int indentation);
		void tab();
		void clear_tabs();
		void newline();
		void add_placeholder(const std::string &mark);
		void write(const std::string& text);
		void write(const std::string& text, const std::string& replacement);
		void write_line(const std::string& text);
		void write_line(const std::string& text, const std::string& replacement);
		void begin_write_on_placeholder(const std::string& placeholder);
		void end_write_on_placeholder();
		void print();
		void indent();
		void unindent();
		void save();
		void restore();
		void save_placeholder_text_cursor(const std::string &placeholder, const std::string& text_end);
		void restore_placeholder_text_cursor(unsigned long diff_in_position);
	private:
		std::stack<std::unique_ptr<PlaceholderTextCursor> > saved_placeholder_text_cursors;
		std::vector<unsigned int> tabs;
		std::vector<std::string> current_placeholders;
		std::map<std::string, std::unique_ptr<TextCursor> > placeholders;
		std::stack<std::unique_ptr<TextAndTextCursor> > saved_text_cursors;
		unsigned int window_width;
		unsigned long position;
		unsigned long column;
		unsigned int indentation;
		const unsigned int indentation_step;

		void
        print_indentation();
	};

}// Lya

#endif // LYA_TEXTWRITER_H
