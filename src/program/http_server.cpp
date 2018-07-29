#include <program/http_server.h>
#include <program/http_parser.h>
#include <program/http_response.h>
#include <program/graphql/graphql_schema.h>
#include <program/graphql/graphql_executor.h>
#include <lib/memory_pool.h>
#include <lib/utils.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <json/json.h>
#include <uv.h>
#include <iostream>

using namespace boost::filesystem;

namespace flashpoint::program {

void
on_write_cb(uv_write_t* write_request, int status)
{
    if (status < 0) {
        std::cerr << uv_err_name(status) << std::endl;
    }
    delete write_request;
}

void
write_to_socket(Client* client, char* buf, size_t len) {
    if(len <= 0) {
        return;
    }
    uv_buf_t uvbuf = uv_buf_init(buf, len);
    uv_write_t* write_request = (uv_write_t*)malloc(sizeof(uv_write_t));
    int r = uv_write(write_request, (uv_stream_t*)client->socket, &uvbuf, 1, on_write_cb);
    if(r < 0) {
        printf("ERROR: write_to_socket erro");
    }
}

void
flush_write_bio(Client *client)
{
    char buf[1024 * 16];
    int bytes_read = 0;
    while((bytes_read = BIO_read(client->write_bio, buf, sizeof(buf))) > 0) {
        write_to_socket(client, buf, bytes_read);
    }
}

void
handle_error(Client* client, int status)
{
    printf("ERROR: %s\n", uv_strerror(status));
}

void
check_outgoing_application_data(Client* c) {
    if (SSL_is_init_finished(c->ssl)) {
        if(c->buffer_out.size() > 0) {
//            std::copy(c->buffer_out.begin(), c->buffer_out.end(), std::ostream_iterator<char>(std::cout,""));
            int r = SSL_write(c->ssl, &c->buffer_out[0], c->buffer_out.size());
            c->buffer_out.clear();
            handle_error(c, r);
            flush_write_bio(c);
        }
    }
}


void
on_read(uv_stream_t *client_stream, ssize_t length, const uv_buf_t *buf)
{
    Client* client = static_cast<Client*>(client_stream->data);
    if (length > 0) {
        if (!SSL_is_init_finished(client->ssl)) {
            BIO_write(client->read_bio, buf->base, length);
            SSL_accept(client->ssl);
            flush_write_bio(client);
            return;
        }
        BIO_write(client->read_bio, buf->base, length);
        char read_buffer[1024 * 10];
        int r = SSL_read(client->ssl, read_buffer, sizeof(read_buffer));
        if (r < 0) {
            handle_error(client, r);
        }
        else if (r > 0) {
            HttpParser http_parser(read_buffer, r);
            std::unique_ptr<HttpRequest> request = http_parser.parse();
            if (request == nullptr) {
                uv_close((uv_handle_t*)client->socket, NULL);
                return;
            }
            auto header_it = request->headers.find(HttpHeader::Accept);
            auto memory_pool = client->server->memory_pool;
            auto ticket = memory_pool->take_ticket();
            std::vector<Glib::ustring> source;
            try {
                if (request->body) {
                    GraphQlSchema schema("type Query { field: Int }", memory_pool, ticket);
                    GraphQlExecutor executor(memory_pool, ticket);
                    executor.add_schema(schema);
                    Json::Reader json_reader;
                    Json::Value request_body;
                    json_reader.parse(request->body, request_body);
                    std::string graphql_query = request_body["query"].asString();
                    auto definition = executor.execute(graphql_query);
                    if (!definition->diagnostics.empty()) {
                        std::cout << "Some error occurred" << std::endl;
                    }
                    for (const auto& operation : definition->operation_definitions) {
                        if (operation->operation_type == OperationType::Query) {

                        }
                    }
                }
            }
            catch (std::exception& ex) {
                std::cerr << ex.what() << std::endl;
            }
            HttpResponse response;
            response.header(HttpHeader::Host, "localhost:8000");
            response.header(HttpHeader::ContentType, "application/json");
            response.header(HttpHeader::Accept, "*/*");
            response.body = "Hello world";
            response.status(200);

            int r = SSL_write(client->ssl, response.to_buffer(), response.size());
            if (r < 0) {
                return handle_error(client, r);
            }
            flush_write_bio(client);
        }
    }

    if (buf->base) {
        delete buf->base;
    }
}

void
on_new_connection(uv_stream_t* server, int status)
{
    if (status < 0) {
        std::fprintf(stderr, "New connection error %s\n", uv_strerror(status));
    }
    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    Client* c = static_cast<Client*>(server->data);
    c->ssl = SSL_new(c->server->ssl_ctx);
    c->read_bio = BIO_new(BIO_s_mem());
    c->write_bio = BIO_new(BIO_s_mem());
    SSL_set_bio(c->ssl, c->read_bio, c->write_bio);
    c->socket = client;
    uv_tcp_init(c->server->loop, client);
    client->data = c;
    if (uv_accept(server, (uv_stream_t*)client) == 0) {
        int r = uv_read_start((uv_stream_t *) client, alloc_buffer, on_read);
        if(r == -1) {
            printf("ERROR: uv_read_start error: %s\n", uv_strerror(r));
            ::exit(0);
        }
    }
    else {
        uv_close((uv_handle_t*)client, NULL);
    }
}


void
handle_signal(uv_signal_t* signal, int signum)
{
    uv_loop_close(signal->loop);
}


void
alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void
on_interval(uv_timer_t* handle)
{
    auto http_server = static_cast<HttpServer*>(handle->data);

    // If parent is dead(parent id is changed), kill the server.
    if (getppid() != http_server->parent_pid) {
        http_server->close();
    }
}

HttpServer::HttpServer(uv_loop_t* loop):
    loop(loop)
{ }

void
HttpServer::listen(const char *host, unsigned int port)
{
    SSL_library_init();
    SSL_load_error_strings();

    parent_pid = getppid();
    Client* c = new Client;
    c->server = this;
    setup_security_context();
    memory_pool = new MemoryPool(1024 * 4 * 10000, 1024 * 4);

    uv_signal_t* signal = (uv_signal_t*)malloc(sizeof(uv_signal_t));
    uv_signal_init(loop, signal);
    uv_signal_start(signal, handle_signal, SIGTERM);
    uv_signal_start(signal, handle_signal, SIGINT);
    uv_signal_start(signal, handle_signal, SIGHUP);
    uv_timer_t* timer_request = (uv_timer_t*)malloc(sizeof(uv_timer_t));
    uv_timer_init(loop, timer_request);
    timer_request->data = this;
    uv_timer_start(timer_request, on_interval, 0, 0);
    uv_tcp_t* server = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, server);
    server->data = c;
    sockaddr_in addr;
    uv_ip4_addr(host, port, &addr);
    uv_tcp_bind(server, (sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t*)server, 128, on_new_connection);
    if (r) {
        std::fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    }
}

void
HttpServer::close()
{
    uv_loop_close(loop);
}

void
HttpServer::setup_security_context()
{
    ssl_ctx = SSL_CTX_new(TLSv1_2_server_method());
    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv3);
    SSL_CTX_set_options(ssl_ctx, SSL_OP_SINGLE_ECDH_USE);
    SSL_CTX_set_options(ssl_ctx, SSL_OP_SINGLE_DH_USE);
    SSL_CTX_set_options(ssl_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
    EC_KEY *ecdh = EC_KEY_new_by_curve_name (NID_X9_62_prime256v1);
    if (!ecdh) {
        throw std::logic_error("Could not generate elliptic curve diffie hellman key.");
    }
    if (1 != SSL_CTX_set_tmp_ecdh (ssl_ctx, ecdh)) {
        throw std::logic_error("Could not set elliptic curve diffie hellman key.");
    }
    EC_KEY_free(ecdh);

    path cert = resolve_paths(root_dir(), "certs/cert.pem");
    path key = resolve_paths(root_dir(), "certs/key.pem").string().c_str();
    const char* cert_path = const_cast<char *>(cert.c_str());
    const char* key_path = const_cast<char *>(key.c_str());
    SSL_CTX_use_certificate_file(ssl_ctx, cert_path, SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ssl_ctx, key_path, SSL_FILETYPE_PEM);

    const char* cipher_list =
        "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:"
        "DHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-SHA256:"
        "DHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:DHE-RSA-AES256-SHA384:"
        "ECDHE-RSA-AES256-SHA256:DHE-RSA-AES256-SHA256:HIGH:!aNULL:!eNULL:"
        "!EXPORT:!DES:!RC4:!MD5:!PSK:!SRP:!CAMELLIA";
    SSL_CTX_set_cipher_list(ssl_ctx, cipher_list);
}

}