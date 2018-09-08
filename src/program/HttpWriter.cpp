#include <iostream>
#include <openssl/ssl.h>
#include "HttpWriter.h"

namespace flashpoint {
static constexpr int const& SIZE_T_LEN = ((sizeof(size_t) * CHAR_BIT + 2) / 3 + 1 );

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
    auto length = strlen(text);
    for (std::size_t i = 0; i < length; i++) {
        if (position_ == buffer_size_) {
            FlushBuffer();
        }
        write_buffer_[position_] = text[i];
        position_++;
    }
}


void
HttpWriter::Write(const char *text, std::size_t length)
{
    for (std::size_t i = 0; i < length; i++) {
        if (position_ == buffer_size_) {
            FlushBuffer();
        }
        write_buffer_[position_] = text[i];
        position_++;
    }
}

void
HttpWriter::ScheduleBodyWrite(const char *text)
{
    auto length = strlen(text);
    content_length_ += length;
    scheduled_body_writes.push_back(new TextSpan(text, length));
}

void
HttpWriter::CommitBodyWrite()
{
    char *content_length = new char[SIZE_T_LEN];
    sprintf(content_length, "Content-Length: %zu\r\n\r\n", content_length_);
    Write(content_length);
    for (const TextSpan* text_span : scheduled_body_writes) {
        Write(text_span->value, text_span->length);
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
        SSL_write(ssl_handle, write_buffer_, position_);
        char buf[1024 * 16];
        unsigned int bytes_read = 0;
        while ((bytes_read = static_cast<unsigned int>(BIO_read(ssl_handle->wbio, buf, sizeof(buf)))) != 0) {
            auto write_request = (uv_write_t*)malloc(sizeof(uv_write_t));
            write_request->data = this;
            uv_buf_t buffer = uv_buf_init(buf, bytes_read);
            int r = uv_write(write_request, (uv_stream_t*)tcp_handle, &buffer, 1, HttpWriterHelperMethods::OnWriteEnd);
            if (r < 0) {
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
