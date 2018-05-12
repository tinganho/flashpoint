//
// Created by Tingan Ho on 2017-10-14.
//

#include "text_writer.h"
#include "utils.h"
#include <cassert>
#include <glibmm/ustring.h>
#include <algorithm>
#include <iostream>

namespace flashpoint::lib {

	TextWriter::TextWriter():
		column(0),
		indentation(0),
		tabs(),
		position(0),
		indentation_step(4),
		text(new std::string(""))
    {
//		if (getenv("COLUMNS") != NULL) {
//			window_width = *(int *)(getenv("COLUMNS"));
//		}
//		else {
//	#ifdef __unix__
//			struct winsize w;
//	        ioctl(0, TIOCGWINSZ, &w);
//	        window_width = w.ws_col;
//	#endif
//		}
	}

	void TextWriter::indent()
	{
		indentation += indentation_step;
	}

	void TextWriter::unindent()
	{
		indentation -= indentation_step;
	}

	void TextWriter::print_indentation()
	{
		for (int i = 0; i < indentation; i++) {
			*text += " ";
			column += 1;
			position += 1;
		}
	}

	void TextWriter::add_tab(unsigned int indentation)
	{
	    tabs.push_back(indentation);
	}

	void TextWriter::tab()
	{
	    for (int i_tab = 0; i_tab < tabs.size(); i_tab++) {
	        if (column < tabs[i_tab]) {
	            int diff = tabs[i_tab] - column;
	            for (int i_diff = 0; i_diff < diff; i_diff++) {
	                *text += " ";
	            }
	            column += diff;
		        position += diff;
	            break;
	        }
	        else {
	            *text += " ";
		        column += 1;
		        position += 1;
	        }
	    }
	}

	void TextWriter::clear_tabs()
	{
	    tabs.clear();
	}

	void TextWriter::newline()
	{
	    *text += '\n';
	    column = 0;
		position += 1;
	}

	void TextWriter::write(const std::string& t)
	{
		if (column == 0) {
			print_indentation();
		}
		unsigned long size = t.size();
	    *text += t;
	    column += size;
		position += size;
	}

	void TextWriter::write(const std::string& t, const std::string& replacement)
	{
		if (column == 0) {
			print_indentation();
		}
		std::string s = replace_string(t, "{0}", replacement);
		unsigned long size = s.size();
		*text += s;
		column += size;
		position += size;
	}

	void TextWriter::write_line(const std::string& t)
	{
	    write(t);
	    newline();
	}

	void TextWriter::write_line(const std::string& t, const std::string& replacement)
	{
		write(t, replacement);
		newline();
	}

	void TextWriter::begin_write_on_placeholder(const std::string& placeholder)
	{
		if (placeholders.find(placeholder) == placeholders.end()) {
			throw std::invalid_argument("Cannot find placeholder '" + placeholder + "'.");
		}
        if (!current_placeholders.empty() && current_placeholders.back() == placeholder) {
            throw std::invalid_argument("Cannot begin write on an already begun placeholder write '" + placeholder + "'.");
        }
		std::unique_ptr<TextCursor>& placeholder_text_cursor = placeholders[placeholder];
        if (placeholder_text_cursor->position > text->size()) {
            throw std::invalid_argument("Cannot begin writing on placeholder '" + placeholder + "', which begins after the current placeholder '" + current_placeholders.back() + "'.");
        }
		save_placeholder_text_cursor(placeholder, text->substr(placeholder_text_cursor->position));
		position = placeholder_text_cursor->position;
		column = placeholder_text_cursor->column;
		indentation = placeholder_text_cursor->indentation;
		*text = text->substr(0, position);
		current_placeholders.push_back(placeholder);
	}

	void TextWriter::end_write_on_placeholder()
	{
        if (saved_placeholder_text_cursors.empty()) {
            throw std::domain_error("You have ended all started placeholders.");
        }
		std::unique_ptr<PlaceholderTextCursor>& saved_text_cursor = saved_placeholder_text_cursors.top();
        std::unique_ptr<std::string>& placeholder = saved_text_cursor->placeholder;
		std::unique_ptr<TextCursor>& placeholder_text_cursor = placeholders[*placeholder];
		if (saved_text_cursor->position >= placeholder_text_cursor->position) {
			restore_placeholder_text_cursor(position - placeholder_text_cursor->position);
		}
		else {
			restore_placeholder_text_cursor(0);
		}
		placeholders.erase(*placeholder);
        auto r = std::remove(current_placeholders.begin(), current_placeholders.end(), *placeholder);
		current_placeholders.erase(r, current_placeholders.end());
        saved_placeholder_text_cursors.pop();
	}

	void TextWriter::save()
	{
		std::unique_ptr<std::string> tmp_text = std::make_unique<std::string>(*text);
		auto saved_text_cursor = std::make_unique<TextAndTextCursor>(
			position,
			column,
			indentation,
			tmp_text
		);
        saved_text_cursors.push(std::move(saved_text_cursor));
	}

	void TextWriter::restore()
	{
        if (saved_text_cursors.empty()) {
            throw std::domain_error("You have restored all saves.");
        }
        std::unique_ptr<TextAndTextCursor>& saved_text_cursor = saved_text_cursors.top();
		position = saved_text_cursor->position;
		column = saved_text_cursor->column;
		text = std::move(saved_text_cursor->text);
        saved_text_cursors.pop();
	}

	void TextWriter::save_placeholder_text_cursor(const std::string& placeholder, const std::string& text_end)
	{
		std::unique_ptr<std::string> _text_end(new std::string(text_end));
        std::unique_ptr<std::string> _placeholder(new std::string(placeholder));
		std::unique_ptr<PlaceholderTextCursor> placeholder_text_cursor = std::make_unique<PlaceholderTextCursor>(
			position,
			column,
			indentation,
			_placeholder,
			_text_end
		);
		saved_placeholder_text_cursors.push(std::move(placeholder_text_cursor));
	}

	void TextWriter::restore_placeholder_text_cursor(unsigned long diff_in_position)
	{
		std::unique_ptr<PlaceholderTextCursor>& placeholder_text_cursor = saved_placeholder_text_cursors.top();
		position = placeholder_text_cursor->position + diff_in_position;
		column = placeholder_text_cursor->column;
		indentation = placeholder_text_cursor->indentation;
        *text += *placeholder_text_cursor->text_end;
	}

	void TextWriter::print()
	{
	    std::cout << *text;
	}

	void TextWriter::add_placeholder(const std::string& m)
	{
		if (placeholders.find(m) != placeholders.end()) {
			throw std::invalid_argument("The placeholder '" + m + "' is already added.");
		}
        if (!current_placeholders.empty()) {
            throw std::domain_error("Cannot add a placeholder when writing to one.");
        }
		placeholders[m] = std::make_unique<TextCursor>(
			position,
			column,
			indentation
		);
	}
} // Lya