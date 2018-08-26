#ifndef FLASHPOINT_HTTP_RESPONSE_H
#define FLASHPOINT_HTTP_RESPONSE_H

#include <program/http_parser.h>
#include <program/http_scanner.h>
#include <program/http_server.h>
#include <unordered_map>
#include <openssl/ssl.h>
#include <exception>
#include <uv.h>

#define BUFFER_SIZE 1024

using namespace flashpoint::program;

namespace flashpoint {

enum class ContentType {
    ApplicationJson,
};

// See StackOverflow replies to this answer for important commentary about inheriting from std::allocator before replicating this code.
template <typename T>
class mmap_allocator: public std::allocator<T>
{
public:
    typedef size_t size_type;
    typedef T* pointer;
    typedef const T* const_pointer;

    template<typename _Tp1>
    struct rebind
    {
        typedef mmap_allocator<_Tp1> other;
    };

    pointer allocate(size_type n, const void *hint=0)
    {
        fprintf(stderr, "Alloc %d bytes.\n", n*sizeof(T));
        return std::allocator<T>::allocate(n, hint);
    }

    void deallocate(pointer p, size_type n)
    {
        fprintf(stderr, "Dealloc %zu bytes (%p).\n", n*sizeof(T), p);
        return std::allocator<T>::deallocate(p, n);
    }

    mmap_allocator() throw(): std::allocator<T>() { fprintf(stderr, "Hello allocator!\n"); }
    mmap_allocator(const mmap_allocator &a) throw(): std::allocator<T>(a) { }
    template <class U>
    mmap_allocator(const mmap_allocator<U> &a) throw(): std::allocator<T>(a) { }
    ~mmap_allocator() throw() { }
};

class HttpHeaderWriter
{
public:
    HttpHeaderWriter(char* header);
};

class HttpWriter
{
public:
    HttpWriter(uv_tcp_t *tcp_handle, SSL *ssl_handle);

    void
    Write(const char *text);

    template<typename ...Args>
    void
    Write(const char*, Args ...args);

    void
    WriteLine();

    void
    WriteLine(const char *text);

    template<typename ...Args>
    void
    WriteLine(const char*, Args ...args);

    void
    WriteRequest(HttpMethod method, const char *path);

    void End();

    void(*Read)(uv_stream_t* tcp_handle, ssize_t nread, const uv_buf_t* buf);

    bool is_end = false;
    SSL* ssl_handle;
    uv_tcp_t *tcp_handle;

private:
    bool use_ssl_;
    char* write_buffer_;
    std::size_t buffer_size_ = 4096;
    std::size_t position_ = 0;
    std::map<HttpHeader, const char*> headers;
    std::map<const char*, const char*> custom_headers;

    void FlushBuffer();

    void WriteToSocket();
};

template<typename ...Args>
void
HttpWriter::Write(const char *text, Args ...args)
{
    Write(text);
    Write(args...);
}

template<typename ...Args>
void
HttpWriter::WriteLine(const char *text, Args ...args)
{
    Write(text);
    Write(args...);
    Write("\r\n");
}

}



#endif //FLASHPOINT_HTTP_RESPONSE_H
