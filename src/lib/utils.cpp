
#include "utils.h"
#include <unistd.h>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
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

    bool path_exists(const boost::filesystem::path& path) {
        return boost::filesystem::exists(path);
    }

    char* read_file(const boost::filesystem::path& filename) {
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

    void write_file(boost::filesystem::path &filename, const char* content) {
        std::ofstream file;
        file.open(filename.string());
        file.write(content, strlen(content));
        file.close();
    }

    void remove_folder(const std::string &path) {
        boost::filesystem::remove_all(boost::filesystem::path(path));
    }

    bool copy_folder(const boost::filesystem::path& source, const boost::filesystem::path& destination) {
        try {
            if (!boost::filesystem::exists(source) || !boost::filesystem::is_directory(source)) {
                std::cerr << "Source directory '" << source.string()
                          << "' does not exist or is not a directory." << '\n';
                return false;
            }
            if (boost::filesystem::exists(destination)) {
                std::cerr << "Destination directory '" << destination.string()
                          << "' already exists." << '\n';
                return false;
            }
            if (!boost::filesystem::create_directory(destination)) {
                std::cerr << "Unable to create destination directory '" << destination.string() << "'.\n";
                return false;
            }
        }
        catch (boost::filesystem::filesystem_error const & e) {
            std::cerr << e.what() << '\n';
            return false;
        }
        for (boost::filesystem::directory_iterator file(source); file != boost::filesystem::directory_iterator(); ++file) {
            try {
                boost::filesystem::path current(file->path());
                if (boost::filesystem::is_directory(current)) {
                    if (!copy_folder(current, destination / current.filename())) {
                        return false;
                    }
                }
                else {
                    boost::filesystem::copy_file(current, destination / current.filename());
                }
            }
            catch (boost::filesystem::filesystem_error const & e) {
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

    void create_folder(boost::filesystem::path &folder) {
        boost::filesystem::create_directories(folder);
    }

    std::vector<std::string> find_files(const std::string& pattern) {
        glob::Glob glob(pattern);
        std::vector<std::string> files;
        while (glob) {
            const std::string path = pattern.substr(0, pattern.find_last_of('/'));
            files.push_back(path + '/' + glob.GetFileName());
            glob.Next();
        }
        return files;
    }

    std::vector<std::string> find_files(const std::string& pattern, const std::string& cwd) {
        if (cwd.front() != '/') {
            throw std::invalid_argument("Utils::find_files: Current working directory 'p_cwd', must be an absolute path. Got '" + cwd + "'.");
        }
        std::string _cwd = cwd;
        if (cwd.back() != '/') {
            _cwd = _cwd + '/';
        }
        return find_files(_cwd + pattern);
    }

    std::string resolve_paths(const std::string& path1, const std::string& path2) {
        boost::filesystem::path p1 (path1);
        return boost::filesystem::canonical(p1 / path2).string();
    }

    std::string resolve_paths(const std::string& path1, const std::string& path2, const std::string& path3) {
        boost::filesystem::path p1 (path1);
        boost::filesystem::path p2 (path2);
        boost::filesystem::path p3 (path3);
        return boost::filesystem::canonical(p1 / p2 / p3).string();
    }

    std::string root_path(const std::string& path) {
        std::string exec_path = get_exec_path();
        return resolve_paths(exec_path, "../") + "/";
    }

    std::string root_path() {
        return root_path("");
    }

    std::string get_cwd() {
        boost::filesystem::path full_path(boost::filesystem::current_path());
        return full_path.string() + "/";
    }

    std::string get_exec_path() {
#ifdef WINDOWS
        char result[MAX_PATH];
		return string(result, GetModuleFileName(NULL, result, MAX_PATH));
#else
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        return std::string(result, (count > 0) ? count : 0);
#endif
    }

    std::vector<std::string> to_vector_of_strings(const Json::Value& vec) {
        std::vector<std::string> res;
        for (const Json::Value& item : vec) {
            res.push_back(item.asString());
        }
        return res;
    }

    template<typename Out>
    void split(const std::string& s, char delimiter, Out result) {
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while (getline(ss, item, delimiter)) {
            *(result++) = item;
        }
    }

    std::vector<std::string> split_string(const std::string& s, char delimiter) {
        std::vector<std::string> elements;
        split(s, delimiter, back_inserter(elements));
        return elements;
    }
} // Lya::Utils
