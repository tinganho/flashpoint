
#include "utils.h"
#include <unistd.h>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#ifdef WINDOWS
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif
#include <codecvt>
#include <boost/regex.hpp>
#include <json/json.h>

using boost::asio::ip::tcp;

namespace flashpoint::lib {

std::string execute_command(const std::string& command) {
    char buffer[128];
    std::string result = "";
#if defined(__APPLE__) || defined(__linux__)
    std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (!feof(pipe.get())) {
        if (fgets(buffer, 128, pipe.get()) != NULL) {
            result += buffer;
        }
    }
#endif
    return result;
}

std::string execute_command(const std::string& command, const std::string& cwd) {
    return execute_command("cd " + cwd + " && " + command);
}

void println(const std::string& text) {
    std::cout << text << std::endl;
}

void println(const std::string& text1, const std::string& text2) {
    std::cout << text1 << text2 << std::endl;
}

void println(const std::string& text1, const std::string& text2, const std::string& text3) {
    std::cout << text1 << text2 << text3 << std::endl;
}

void sleep(int ms) {
    usleep(ms);
}

bool path_exists(const path& path) {
    return exists(path);
}

char* read_file(const path& filename) {
    std::ifstream file(filename.string(), std::ios::binary);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        int size = file.tellg();
        char* content = new char[size + 1];
        file.seekg(0, std::ios::beg);
        file.read(content, size);
        content[size] = '\0';
        file.close();
        return content;
    }
    else {
        throw std::logic_error("Unable to open file" + filename.string());
    }
}

void write_file(const path& filename, const char* content) {
    std::ofstream file;
    file.open(filename.string());
    file.write(content, strlen(content));
    file.close();
}

void remove_folder(const path& path) {
    boost::filesystem::remove_all(path);
}

bool copy_folder(const path& source, const path& destination) {
    try {
        if (!exists(source) || !is_directory(source)) {
            std::cerr << "Source directory '" << source.string()
                      << "' does not exist or is not a directory." << '\n';
            return false;
        }
        if (exists(destination)) {
            std::cerr << "Destination directory '" << destination.string()
                      << "' already exists." << '\n';
            return false;
        }
        if (!create_directory(destination)) {
            std::cerr << "Unable to create destination directory '" << destination.string() << "'.\n";
            return false;
        }
    }
    catch (filesystem_error const & e) {
        std::cerr << e.what() << '\n';
        return false;
    }
    for (directory_iterator file(source); file != directory_iterator(); ++file) {
        try {
            path current(file->path());
            if (is_directory(current)) {
                if (!copy_folder(current, destination / current.filename())) {
                    return false;
                }
            }
            else {
                copy_file(current, destination / current.filename());
            }
        }
        catch (filesystem_error const & e) {
            std::cerr << e.what() << '\n';
        }
    }
    return true;
}

std::string folder_path(const std::string& path) {
#if defined _WIN32 || defined __CYGWIN__
    return path.substr(0, path.find_last_of("\\"));
#else
    return path.substr(0, path.find_last_of("/"));
#endif
}

std::string replace_string(const std::string& target, const std::string& pattern, const std::string& replacement) {
    return boost::replace_all_copy(target, pattern, replacement);
}

std::string replace_all(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

namespace Debug {
    void fail(std::string err) {
        throw std::logic_error(err);
    }
}

void create_folder(const path& folder) {
    create_directories(folder);
}

std::string join_vector(std::vector<std::string> vec, std::string delimiter) {
    std::stringstream ss;
    for (size_t i = 0; i < vec.size(); ++i) {
        if(i != 0)
            ss << delimiter;
        ss << vec[i];
    }
    return ss.str();
}

std::vector<std::string> find_files(const path& pattern) {
    std::string _pattern = pattern.string();
    glob::Glob glob(_pattern);
    std::vector<std::string> files;
    while (glob) {
        const std::string path = _pattern.substr(0, _pattern.find_last_of('/'));
        files.push_back(path + '/' + glob.GetFileName());
        glob.Next();
    }
    return files;
}

std::vector<std::string> find_files(const path& pattern, const std::string& cwd) {
    if (cwd.front() != '/') {
        throw std::invalid_argument("Utils::find_files: Current working directory 'p_cwd', must be an absolute path. Got '" + cwd + "'.");
    }
    std::string _cwd = cwd;
    if (cwd.back() != '/') {
        _cwd = _cwd + '/';
    }
    return find_files(_cwd / pattern);
}

path resolve_paths(const path& _path1, const path& _path2) {
    return weakly_canonical(_path1 / _path2);
}

path resolve_paths(const path& _path1, const path& _path2, const path& _path3) {
    return weakly_canonical(_path1 / _path2 / _path3);
}

path root_path(const path& _path) {
    return weakly_canonical(root_dir() / _path);
}

path root_dir() {
    return weakly_canonical(resolve_paths(boost::dll::program_location(), "../../"));
}

std::vector<std::string> to_vector_of_strings(const Json::Value& vec) {
    std::vector<std::string> res;
    for (const Json::Value& item : vec) {
        res.push_back(item.asString());
    }
    return res;
}

char* ConcatChars(char *dest, const char *src) {
    while (*dest) {
        dest++;
    }
    while ((*dest++ = *src++));
    return --dest;
}


} // Lya::Utils
