//
// Created by Tingan Ho on 2018-04-08.
//

#include <iostream>
#include <vector>
#include "http_parser.h"

namespace flash::lib {

    static std::vector<std::tuple<const char*, int>> should_parse_header = {
        { "Host", sizeof("Host") - 1 }
    };

    HttpParser::HttpParser(char* text, unsigned int size):
        scanner(text, size)
    {

    }

    std::unique_ptr<HttpRequest> HttpParser::parse()
    {
        auto [method, path, query] = parse_request_line();
        std::map<HttpHeader, char*> headers = parse_headers();
        std::unique_ptr<HttpRequest> request(new HttpRequest {
            method,
            path,
            query,
            headers,
        });
        return request;
    }

    std::map<HttpHeader, char*> HttpParser::parse_headers() {
        std::map<HttpHeader, char*> headers = {};
        while (true) {
            auto header = scanner.scan_header();
            if (header == HttpHeader::End) {
                break;
            }
            headers.emplace(header, scanner.get_header_value());
        }
        return headers;
    };

    std::map<std::string, std::string> HttpParser::get_graphql_origins()
    {

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