//
// Created by Tingan Ho on 2018-04-26.
//

#ifndef FLASHPOINT_HTTP_RESPONSE_H
#define FLASHPOINT_HTTP_RESPONSE_H

#include <program/http_parser.h>
#include <program/http_scanner.h>
#include <program/http_server.h>
#include <unordered_map>
#include <uv.h>

#define BUFFER_SIZE 1024

namespace flashpoint::program {

    enum class ContentType {
        ApplicationJson,
    };

    class HttpResponse {
    public:
        HttpResponse();
        const char* body;
        void header(HttpHeader header, const char *value);
        void status(int status, const char* reason);
        void status(int status);
        size_t size();
        char* to_buffer();

    private:
        std::vector<char> buffer;
        std::vector<std::pair<HttpHeader, const char*>> headers;
        void write(const char* text);
        void write(const char* text, int length);
        void write_status_line(int status, const char *reason);
        void write_space();
        void write_newline();
        char _text[1024];
        unsigned int position;
        ContentType content_type;
    };
}


#endif //FLASHPOINT_HTTP_RESPONSE_H
