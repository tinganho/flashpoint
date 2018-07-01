#include <string>
#include <iostream>
#include <boost/algorithm/string/replace.hpp>
#include <boost/regex.hpp>
#include <json/json.h>
#include <lib/utils.h>

using namespace flashpoint::lib;

const std::string start_wrap_header_template =
    "// This code is auto generate. Don't edit it!\n"
    "#ifndef {{header_guard}}\n"
    "#define {{header_guard}}\n"
    "\n"
    "#include <program/diagnostic.h>\n"
    "\n"
    "using namespace flashpoint::program;\n"
    "\n"
    "namespace {{namespace}} {\n"
    "\n"
    "class D {\n"
    "public:\n";

const std::string start_wrap_source_template =
    "// This code is auto generate. Don't edit it!\n"
    "#include \"{{header_file}}\"\n"
    "\n"
    "namespace {{namespace}} {\n"
    "\n";

const std::string end_wrap_header_template =
    "};\n"
    "\n"
    "}\n"
    "\n"
    "#endif // {{header_guard}}";

const std::string end_wrap_source_template =
    "\n"
    "} // {{namespace}}\n";

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
    k = boost::regex_replace(k, boost::regex("['|\"|?|:|,|.|(|)]"), "");
    k = boost::regex_replace(k, boost::regex("{(\\d+)}"), "$1");
    k = boost::regex_replace(k, boost::regex("_+"), "_");
    k = boost::regex_replace(k, boost::regex("^_+|_+$"), "");
    boost::match_results<std::string::const_iterator> results;
    return k;
}

std::string remove_comments(std::string json) {
    return boost::regex_replace(json, boost::regex("//.*?\n"), "\n");
}

void generate_diagnostic(
    const char* folder,
    const char* file,
    const std::string& _namespace)
{
    const char* json = read_file(root_dir() / folder / (std::string(file) + ".json"));
    Json::Value diagnostics;
    Json::Reader reader;

    std::string start_wrap_header = boost::regex_replace(start_wrap_header_template, boost::regex("{{header_guard}}"), std::string(file) + "_H");
    start_wrap_header = boost::regex_replace(start_wrap_header, boost::regex("{{namespace}}"), _namespace);

    std::string start_wrap_source =  boost::regex_replace(start_wrap_source_template, boost::regex("{{header_file}}"), std::string(file) + ".h");
    start_wrap_source = boost::regex_replace(start_wrap_source, boost::regex("{{namespace}}"), _namespace);

    std::string end_wrap_header = boost::regex_replace(end_wrap_header_template, boost::regex("{{header_guard}}"), std::string(file) + "_H");

    std::string end_wrap_source =  boost::regex_replace(end_wrap_source_template, boost::regex("{{namespace}}"), _namespace);

    reader.parse(remove_comments(json).c_str(), diagnostics);
    for (Json::ValueIterator it = diagnostics.begin(); it != diagnostics.end(); ++it) {
        auto key = it.key().asString();
        std::string unformatted_key = boost::regex_replace(key, boost::regex("\\\""), "\\\\\\\"");
        std::string formatted_key = format_diagnostic_key(key);
        if (!is_unique(formatted_key)) {
            throw std::invalid_argument("Duplicate formatted key: " + formatted_key + ".");
        }
        start_wrap_header += "    static DiagnosticMessageTemplate " + formatted_key + ";\n";
        start_wrap_source += "    DiagnosticMessageTemplate D::" + formatted_key + " = " + "DiagnosticMessageTemplate { \"" + unformatted_key + "\" };\n";
        keys.push_back(formatted_key);
    }
    start_wrap_header += end_wrap_header;
    start_wrap_source += end_wrap_source;
    write_file(root_dir() / folder / (std::string(file) + ".h"), start_wrap_header.c_str());
    write_file(root_dir() / folder / (std::string(file) + ".cpp"), start_wrap_source.c_str());
    keys.clear();
}

int main() {
    try {
        generate_diagnostic(
            "src/program/graphql",
            "graphql_diagnostics",
            "flashpoint::program"
        );
        generate_diagnostic(
            "src/test",
            "test_case_diagnostics",
            "flashpoint::test"
        );
        std::cout << "Successfully generated new diagnostics." << std::endl;
        return 0;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}