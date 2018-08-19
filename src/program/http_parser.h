#ifndef FLASH_HTTP_PARSER_H
#define FLASH_HTTP_PARSER_H

#include <program/http_scanner.h>
#include <unordered_map>
#include <types.h>
#include <uv.h>

using namespace flashpoint::lib;

namespace flashpoint::program {

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
    char* body;
    uv_stream_t* client_stream;
};

class HttpParser final {
public:

    HttpParser(const char* text, std::size_t length);

    std::unique_ptr<HttpRequest>
    Parse();

    RequestLine
    ParseRequestLine();

    std::map<HttpHeader, char*>
    parse_headers();

    char*
    parse_body(long long length);

private:
    HttpScanner scanner;
    unsigned int length;
};

}


#endif //FLASH_HTTP_PARSER_H
