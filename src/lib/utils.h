
#ifndef UTILS_H
#define UTILS_H

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <exception>
#include <json/json.h>
#include <glob.h>
#include <sstream>
#include "types.h"
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

using namespace boost::filesystem;

namespace flashpoint::lib {
    std::string execute_command(const std::string& command);
    std::string execute_command(const std::string& command, const std::string& cwd);
    std::u32string from_utf8_to_u32(const std::string& text);

    void println(const std::string& text);
    void println(const std::string& text1, const std::string& text2);
    void println(const std::string& text1, const std::string& text2, const std::string& text3);

    bool path_exists(const path& path);

    char* read_file(const path& file);
    void write_file(const path& file, const char* content);

    void create_folder(const path& folder);
    void remove_folder(const path& path);
    std::string folder_path(const std::string &path);
    bool copy_folder(const path& source, const path& destination);

    path resolve_paths(const path& _path1, const path& _path2);
    path resolve_paths(const path& path1, const path& path2, const path& path3);
    path root_path(const path& path1);
    path root_dir();

    std::vector<std::string> find_files(const path& pattern);
    std::vector<std::string> find_files(const path& pattern, const path& cwd);
    std::string join_vector(std::vector<std::string> vec, std::string delimiter);
    std::string replace_string(const std::string& target, const std::string& pattern, const std::string& replacement);
    std::string replace_all(std::string str, const std::string& from, const std::string& to);

    std::string get_cwd();
    std::string get_exec_path();

    std::vector<std::string> to_vector_of_strings(const Json::Value& vec);

    template<typename K, typename V>
    std::map<V, K> reverse_map(const std::map <K, V> &input) {
        typename std::map<V, K> output;
        for (auto it = input.begin(); it != input.end(); it++) {
            output.emplace(it->second, it->first);
        }
        return output;
    }

    template<typename Out>
    void split(const std::string& s, char delimiter, Out result);

    template <typename T>
    std::vector<T> split_string(const T& s, char delimiter);
    template<typename T>
    std::vector<T> split_string(const T &s, const std::string& delimiter);

    template<typename Out>
    void split(const std::string& s, char delimiter, Out result) {
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while (getline(ss, item, delimiter)) {
            *(result++) = item;
        }
    }

    template<typename T>
    std::vector<T> split_string(const T& s, char delimiter) {
        std::vector<std::string> elements;
        split(s, delimiter, back_inserter(elements));
        return elements;
    }

    template<typename T>
    std::vector<T> split_string(const T &s, const std::string& delimiter) {
        std::vector<T> elements;
        std::string::size_type prev_pos = 0, pos = 0;
        while ((pos = s.find(delimiter, pos)) != std::string::npos)
        {
            elements.emplace_back(s.substr(prev_pos, pos-prev_pos));
            prev_pos = pos + delimiter.size();
        }
        elements.emplace_back(s.substr(prev_pos, pos-prev_pos));
        return elements;
    }

    void sleep(int ms);
    namespace Debug {
        void fail(std::string err);
    }

} // flash::lib

#endif // UTILS_H
