//
// Created by Tingan Ho on 2018-04-08.
//

#ifndef FLASH_HTTP_PARSER_H
#define FLASH_HTTP_PARSER_H

#include <program/http_scanner.h>
#include <types.h>

using namespace flashpoint::lib;

namespace flashpoint::program {
    enum class Headers {
        Method =  1 << 1,
        Path =    1 << 2,
        Headers = 1 << 3,
        Body =    1 << 4,
    };

    enum class Version {
        _1_1,
        _2,
    };

    enum class ContentType {

    };

    struct RequestLine {
        HttpMethod method;
        char* path;
        char* query;
    };

    struct HttpRequest {
        HttpMethod method;
        char* path;
        char* query;
        std::map<HttpHeader, char*> headers;
    };

    class HttpParser final {
    public:
        HttpParser(char* text, unsigned int length);
        std::unique_ptr<HttpRequest> parse();
        RequestLine parse_request_line();
        std::map<HttpHeader, char*> parse_headers();
    private:
        HttpScanner scanner;
        unsigned int length;

    };
}


#endif //FLASH_HTTP_PARSER_H
