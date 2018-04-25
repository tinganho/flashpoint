#include <iostream>
#include <vector>
#include <program/http_parser.h>

using namespace flashpoint::lib;

namespace flashpoint::program {

    HttpParser::HttpParser(char* text, unsigned int size):
        scanner(text, size)
    {

    }

    std::unique_ptr<HttpRequest> HttpParser::parse()
    {
        auto [method, path, query] = parse_request_line();
        std::map<HttpHeader, char*> headers = parse_headers();
        auto it = headers.find(HttpHeader::ContentLength);
        if (it != headers.end()) {
            parse_body(std::atoll(it->second));
        }

        std::unique_ptr<HttpRequest> request(new HttpRequest {
            method,
            path,
            query,
            headers,
        });
        return request;
    }

    void HttpParser::parse_body(long long length)
    {
        const char* body = scanner.scan_body(length);
        const char* error;
    }

    std::map<HttpHeader, char*> HttpParser::parse_headers()
    {
        std::map<HttpHeader, char*> headers = {};
        while (true) {
            auto header = scanner.scan_header();
            if (header == HttpHeader::End) {
                break;
            }
            headers.emplace(header, scanner.get_header_value());
        }
        return headers;
    }

    RequestLine HttpParser::parse_request_line()
    {
        HttpMethod method = scanner.scan_method();
        scanner.scan_expected(Character::Space);
        char* path = scanner.scan_absolute_path();
        char* query = scanner.scan_query();
        scanner.scan_expected(Character::Space);
        scanner.scan_http_version();
        scanner.scan_expected(Character::CarriageReturn);
        scanner.scan_expected(Character::LineFeed);
        return RequestLine {
            method,
            path,
            query,
        };
    }
}