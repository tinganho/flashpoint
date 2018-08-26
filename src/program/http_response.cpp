#include <iostream>
#include <openssl/ssl.h>
#include "http_scanner.h"
#include "http_response.h"

namespace flashpoint {

namespace HttpWriterHelperMethods {

void
OnWriteEnd(uv_write_t *write_request, int status)
{
    if (status == -1) {
        fprintf(stderr, "error on_write_end");
        return;
    }

    delete write_request;
}

}

HttpWriter::HttpWriter(uv_tcp_t *tcp_handle, SSL *ssl_handle)
    : tcp_handle(tcp_handle),
      ssl_handle(ssl_handle)
{
    if (ssl_handle != nullptr) {
        use_ssl_ = true;
    }
    write_buffer_ = new char[buffer_size_];
}

void
HttpWriter::WriteRequest(
    HttpMethod method,
    const char *path)
{
    switch (method) {
        case HttpMethod::Get:
            Write("GET ");
            break;
        case HttpMethod::Post:
            Write("POST ");
            break;
        case HttpMethod::Patch:
            Write("PATCH ");
            break;
        case HttpMethod::Head:
            Write("HEAD ");
            break;
        default:;
    }
    Write(path);
    Write(" HTTP/1.1\r\n");
}

void
HttpWriter::Write(const char *text)
{
    for (std::size_t i = 0; i < strlen(text); i++) {
        if (position_ == buffer_size_) {
            FlushBuffer();
        }
        write_buffer_[position_] = text[i];
        position_++;
    }
}

void
HttpWriter::WriteLine()
{
    Write("\r\n");
}

void
HttpWriter::WriteLine(const char *text)
{
    Write(text);
    Write("\r\n");
}

void
HttpWriter::FlushBuffer()
{
    if (position_ == 0) {
        return;
    }
    if (use_ssl_) {
        SSL_write(ssl_handle, write_buffer_, (int)position_);
        char buf[1024 * 16];
        int bytes_read = 0;
        while ((bytes_read = BIO_read(ssl_handle->wbio, buf, sizeof(buf))) > 0) {
            auto write_request = (uv_write_t*)malloc(sizeof(uv_write_t));
            write_request->data = this;
            uv_buf_t buffer = uv_buf_init(buf, (unsigned int)bytes_read);
            int r = uv_write(write_request, (uv_stream_t*)tcp_handle, &buffer, 1, HttpWriterHelperMethods::OnWriteEnd);
            if(r < 0) {
                printf("ERROR: WriteToSocket erro");
                return;
            }
        }
    }
    else {
        WriteToSocket();
    }
    position_ = 0;
}

void
HttpWriter::WriteToSocket()
{
    uv_write_t *write_request = (uv_write_t*)malloc(sizeof(uv_write_t));
    write_request->data = this;
    uv_buf_t buffer = uv_buf_init(write_buffer_, (int)position_);
    int r = uv_write(write_request, (uv_stream_t*)tcp_handle, &buffer, 1, HttpWriterHelperMethods::OnWriteEnd);
    if(r < 0) {
        printf("ERROR: WriteToSocket erro");
    }
}

void
HttpWriter::End()
{
    FlushBuffer();
    is_end = true;
}

} // flashpoint
