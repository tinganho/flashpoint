#include <string>
#include <iostream>
#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>
#include <json/json.h>
#include <lib/utils.h>

using namespace flashpoint::lib;

const std::string start_wrap_header =
    "// This code is auto generate. Don't edit it!\n"
    "#ifndef DIAGNOSTICS_H\n"
    "#define DIAGNOSTICS_H\n"
    "\n"
    "#include <program/diagnostic.h>\n"
    "\n"
    "namespace flashpoint::program {\n"
    "\n"
    "class D {\n"
    "public:\n";

const std::string start_wrap_source =
    "// This code is auto generate. Don't edit it!\n"
    "#include \"{{header_file}}\"\n"
    "\n"
    "namespace flashpoint::program {\n"
    "\n";

const std::string end_wrap_header =
    "};\n"
    "\n"
    "}\n"
    "\n"
    "#endif // DIAGNOSTICS_H";

const std::string end_wrap_source =
    "\n"
    "} // flashpoint::program\n";

std::vector<std::string> keys = {};

bool is_unique(std::string key) {
    for (auto const& k : keys) {
        if (k == key) {
            return false;
        }
    }
    return true;
}

std::string format_diagnostic_key(std::string key) {
    std::string k = boost::regex_replace(key, boost::regex("\\s+"), "_");
    k = boost::regex_replace(k, boost::regex("[\\.|\\'|:|,|.]"), "");
    k = boost::regex_replace(k, boost::regex("{(\\d+)}"), "$1");
    k = boost::regex_replace(k, boost::regex("_+"), "_");
    k = boost::regex_replace(k, boost::regex("^_+|_+$"), "");
    boost::match_results<std::string::const_iterator> results;
    if (boost::regex_search(k, boost::regex("[^a-zA-Z\\d_]"))) {
        throw std::invalid_argument("Your 'diagnostics.json' file contains non-alpha numeric characters: " + key);
    }

    return k;
}

std::string remove_comments(std::string json) {
    return boost::regex_replace(json, boost::regex("//.*?\n"), "\n");
}

void generate_diagnostic(const char* folder, const char* file) {
    const char* json = read_file(root_dir() / folder / (std::string(file) + ".json"));
    Json::Value diagnostics;
    Json::Reader reader;
    std::string header_file = start_wrap_header;
    std::string source_file = start_wrap_source;
    source_file = boost::regex_replace(source_file, boost::regex("\{\{header_file\}\}"), std::string(file) + ".h");
    reader.parse(remove_comments(json).c_str(), diagnostics);
    for (Json::ValueIterator it = diagnostics.begin(); it != diagnostics.end(); ++it) {
        std::string unformatted_key = it.key().asString();
        std::string formatted_key = format_diagnostic_key(unformatted_key);
        if (!is_unique(formatted_key)) {
            throw std::invalid_argument("Duplicate formatted key: " + formatted_key + ".");
        }
        header_file += "    static DiagnosticMessageTemplate " + formatted_key + ";\n";
        source_file += "    DiagnosticMessageTemplate D::" + formatted_key + " = " + "DiagnosticMessageTemplate { \"" + unformatted_key + "\" };\n";
        keys.push_back(formatted_key);
    }
    header_file += end_wrap_header;
    source_file += end_wrap_source;
    write_file(root_dir() / folder / (std::string(file) + ".h"), header_file.c_str());
    write_file(root_dir() / folder / (std::string(file) + ".cpp"), source_file.c_str());
}

int main() {
    try {
        generate_diagnostic("src/program/graphql", "graphql_diagnostics");
        std::cout << "Successfully generated new diagnostics." << std::endl;
        return 0;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}