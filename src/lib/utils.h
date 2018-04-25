
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
#include "types.h"
#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS


namespace flashpoint::lib {
    std::string execute_command(const std::string& command);
    std::string execute_command(const std::string& command, const std::string& cwd);
    std::u32string from_utf8_to_u32(const std::string& text);

    void println(const std::string& text);
    void println(const std::string& text1, const std::string& text2);
    void println(const std::string& text1, const std::string& text2, const std::string& text3);

    bool path_exists(const boost::filesystem::path& path);

    char* read_file(const boost::filesystem::path& file);
    void write_file(boost::filesystem::path& file, const char* content);

    void create_folder(boost::filesystem::path &folder);
    void remove_folder(const std::string& path);
    std::string folder_path(const std::string &path);
    bool copy_folder(const boost::filesystem::path& source, const boost::filesystem::path& destination);

    std::string resolve_paths(const std::string& path1, const std::string& path2);
    std::string resolve_paths(const std::string& path1, const std::string& path2, const std::string& path3);
    std::string root_path(const std::string& path);
    std::string root_path();

    std::vector<std::string> find_files(const std::string& pattern);
    std::vector<std::string> find_files(const std::string& pattern, const std::string& cwd);

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
    std::vector<std::string> split_string(const std::string& s, char delimiter);

    void sleep(int ms);
    namespace Debug {
        void fail(std::string err);
    }

} // flash::lib

#endif // UTILS_H
