#include <iostream>
#include <vector>
#include <unordered_map>
#include <program/http_parser.h>
#include <lib/character.h>

using namespace flashpoint::lib;

namespace flashpoint::program {

HttpParser::HttpParser(char* text, unsigned int size):
    scanner(text, size)
{ }

std::unique_ptr<HttpRequest>
HttpParser::parse()
{
    auto [method, path, query] = parse_request_line();
    std::map<HttpHeader, char*> headers = parse_headers();
    char* body = nullptr;
    if (method != Get) {
        const char* header = headers[HttpHeader::ContentLength];
        if (header != NULL) {
            body = parse_body(std::atoll(header));
        }
    }

    std::unique_ptr<HttpRequest> request(new HttpRequest {
        method,
        path,
        query,
        headers,
        body,
        nullptr,
    });
    return request;
}

char*
HttpParser::parse_body(long long length)
{
    return scanner.scan_body(length);
}

std::map<HttpHeader, char*>
HttpParser::parse_headers()
{
    std::map<HttpHeader, char*> headers = {};
    while (true) {
        auto header = scanner.scan_header();
        if (header == HttpHeader::End) {
            break;
        }
        headers[header] = scanner.get_header_value();
    }
    return headers;
}

RequestLine
HttpParser::parse_request_line()
{
    HttpMethod method = scanner.scan_method();
    scanner.scan_expected(Character::Space);
    char* path = scanner.scan_absolute_path();
    char* query = scanner.scan_query();
    scanner.scan_expected(Character::Space);
    scanner.scan_http_version();
    scanner.scan_expected(Character::CarriageReturn);
    scanner.scan_expected(Character::NewLine);
    return RequestLine {
        method,
        path,
        query,
    };
}

}