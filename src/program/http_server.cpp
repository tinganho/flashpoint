#include <program/http_server.h>
#include <program/http_parser.h>
#include <program/http_response.h>
#include <program/graphql/graphql_schema.h>
#include <program/graphql/graphql_executor.h>
#include <lib/memory_pool.h>
#include <lib/utils.h>
#include <lib/tcp_client_raw.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/dh.h>
#include <json/json.h>
#include <uv.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace boost::filesystem;

namespace flashpoint {

ExecutableDefinition* ParseRequest(GatewayClient *client, const char *read_buffer, std::size_t size);
void ForwardRequest(ClientRequest* client_request, Field* field);

void AllocateBuffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void on_close(uv_handle_t *handle) {
    printf("closed forward request.");
}

void OnWriteEnd(uv_write_t *write_request, int status) {
    if (status < 0) {
        std::cerr << uv_err_name(status) << std::endl;
    }
    delete write_request;
}

void WriteToSocket(GatewayClient *client, char *buf, size_t len) {
    if(len <= 0) {
        return;
    }
    uv_buf_t uvbuf = uv_buf_init(buf, len);
    auto write_request = (uv_write_t*)malloc(sizeof(uv_write_t));
    int r = uv_write(write_request, (uv_stream_t *) client->tcp_handle, &uvbuf, 1, OnWriteEnd);
    if(r < 0) {
        printf("ERROR: WriteToSocket erro");
    }
}

void FlushWriteBio(GatewayClient *client) {
    char buf[1024 * 16];
    int bytes_read = 0;
    while((bytes_read = BIO_read(client->ssl_handle->wbio, buf, sizeof(buf))) > 0) {
        WriteToSocket(client, buf, bytes_read);
    }
}

void handle_error(GatewayClient* client, int status) {
    printf("ERROR: %s\n", uv_strerror(status));
}

char* mystrcat(char* dest, const char* src) {
    while (*dest) {
        dest++;
    }
    while ((*dest++ = *src++));
    return --dest;
}

void OnForwardRequestRead(uv_stream_t *tcp, ssize_t length, const uv_buf_t *buf) {
    if (length >= 0) {
        printf("read: %s\n", buf->base);
    }
    else {
        uv_close((uv_handle_t*)tcp, on_close);
    }
    free(buf->base);
}

void OnClientConnect(uv_connect_t *connection, int status) {
    if (status == -1) {
        std::cerr << "error on write_end" << std::endl;
    }
    auto client_request = static_cast<ClientRequest*>(connection->data);
    auto gateway_client = client_request->gateway_client;
    uv_stream_t *stream = connection->handle;
    uv_read_start(stream, AllocateBuffer, OnForwardRequestRead);
    for (const auto& field : gateway_client->fields) {
        auto http_writer = HttpWriter((uv_stream_t*)client_request->tcp_handle);
        http_writer.WriteRequest(HttpMethod::Post, client_request->path);
        http_writer.WriteLine("Host: ", client_request->host);
        http_writer.WriteLine("User-Agent: flash");
        http_writer.WriteLine("Accept: */*");
        http_writer.WriteLine("Content-Type: application/json; charset=utf-8");
        http_writer.WriteLine("Content-Length: 24");
        http_writer.WriteLine();
        http_writer.Write("{ \"query\": \"{ field }\" }");
        http_writer.End();
    }
}

void OnResolvedIpv4(uv_getaddrinfo_t *handle, int status, struct addrinfo *res) {
    char ip_addr[17] = { '\0' };
    int r = uv_ip4_name((sockaddr_in*)(res->ai_addr), ip_addr, 16);
    if (r) {
        printf("Error getting ipv4 address: %s.\n", uv_strerror(r));
        return;
    }
    auto request_address = (sockaddr_in*)malloc(sizeof(sockaddr_in));
    auto client_request = (ClientRequest*)handle->data;
    r = uv_ip4_addr(ip_addr, client_request->port, request_address);
    if (r) {
        printf("Error getting ipv4 address: %s.\n", uv_strerror(r));
        return;
    }
    auto connect_request = (uv_connect_t*)malloc(sizeof(uv_connect_t));
    connect_request->data = handle->data;
    client_request->tcp_handle = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(handle->loop, client_request->tcp_handle);
    uv_tcp_keepalive(client_request->tcp_handle, 1, 60);
    uv_tcp_connect(connect_request, client_request->tcp_handle, (sockaddr*)request_address, OnClientConnect);

    uv_freeaddrinfo(res);
}

std::map<const char*, BackendEndpoint, cmp_str> field_to_endpoint = {
    { "field", { "http://localhost:4000/graphql", "localhost:4000", "localhost", 4000, "/graphql"} }
};

void on_read(uv_stream_t *client_stream, ssize_t length, const uv_buf_t *buf) {
    if (length == 0) {
        free(buf->base);
        uv_close((uv_handle_t *) client_stream, on_close);
        return;
    }
    auto gateway_client = static_cast<GatewayClient*>(client_stream->data);
    if (!SSL_is_init_finished(gateway_client->ssl_handle)) {
        BIO_write(gateway_client->ssl_handle->rbio, buf->base, length);
        SSL_accept(gateway_client->ssl_handle);
        FlushWriteBio(gateway_client);
        return;
    }
    char read_buffer[1024 * 10];
    BIO_write(gateway_client->ssl_handle->rbio, buf->base, length);
    int read_size = SSL_read(gateway_client->ssl_handle, read_buffer, sizeof(read_buffer));
    if (read_size < 0) {
        handle_error(gateway_client, read_size);
        return;
    }
    else if (read_size > 0) {
        auto executable_definition = ParseRequest(gateway_client, read_buffer, read_size);
        if (executable_definition == nullptr) {
            return;
        }
        OperationDefinition* operation_definition;
        if (executable_definition->operation_definitions.size() == 1) {
            operation_definition = executable_definition->operation_definitions.at(0);
        }
        else {
            Glib::ustring name = "default_operation";
            auto operation_definitions = executable_definition->operation_definitions;
            auto operation_definition_it = std::find_if(operation_definitions.begin(), operation_definitions.end(), [&](OperationDefinition* operation_definition) -> bool {
                return operation_definition->name->identifier == name;
            });
            if (operation_definition_it == operation_definitions.end()) {
                return;
            }
            operation_definition = *operation_definition_it;
        }
        for (const auto& selection : operation_definition->selection_set->selections) {
            if (selection->kind !=SyntaxKind::S_Field) {
                return;
            }
            auto field = static_cast<Field*>(selection);
            auto name = static_cast<Field*>(selection)->name->identifier;
            auto endpoint_it = field_to_endpoint.find(name.c_str());
            if (endpoint_it != field_to_endpoint.end()) {
                auto backend_endpoint = endpoint_it->second;
                gateway_client->fields.emplace(endpoint_it->first, field);
                gateway_client->fragments = &executable_definition->fragment_definitions;
                auto client_request = new ClientRequest {
                    backend_endpoint.hostname,
                    backend_endpoint.port,
                    backend_endpoint.host,
                    backend_endpoint.path,
                    nullptr,
                    gateway_client,
                };
                ForwardRequest(client_request, field);
            }
            else {
                return;
            }
        }
    }

    if (buf->base) {
        free(buf->base);
    }
}

ExecutableDefinition* ParseRequest(GatewayClient *client, const char *read_buffer, std::size_t size) {
    HttpParser http_parser(read_buffer, size);
    std::unique_ptr<HttpRequest> request = http_parser.Parse();
    if (request == nullptr) {
        uv_close((uv_handle_t*)client->tcp_handle, nullptr);
        return nullptr;
    }
    auto memory_pool = client->server->memory_pool;
    auto ticket = memory_pool->TakeTicket();
    GraphQlSchema schema("type Query { field: Int }", memory_pool, ticket);
    GraphQlExecutor graphql_executor(memory_pool, ticket);
    graphql_executor.add_schema(schema);
    Json::Reader json_reader;
    Json::Value request_body;
    if (request->body != nullptr) {
        json_reader.parse(request->body, request_body);
        std::string graphql_query = request_body["query"].asString();
        return graphql_executor.Execute(graphql_query);
    }
    return nullptr;
}

void ForwardRequest(ClientRequest* client_request, Field* field) {
    auto addrinfo = (uv_getaddrinfo_t*)malloc(sizeof(uv_getaddrinfo_t));
    addrinfo->data = client_request;
    auto gateway_client = client_request->gateway_client;
    auto loop = gateway_client->server->loop;
    int r = uv_getaddrinfo(loop, addrinfo, OnResolvedIpv4, client_request->hostname, "80", NULL);
    if (r) {
        printf("Error at dns request: %s.\n", uv_strerror(r));
        uv_close((uv_handle_t*)gateway_client->tcp_handle, nullptr);
        return;
    }
}

void OnNewConnection(uv_stream_t *server, int status) {
    if (status < 0) {
        std::fprintf(stderr, "New connection error %s\n", uv_strerror(status));
    }
    auto tcp_handle = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    auto gateway_client = static_cast<GatewayClient*>(server->data);
    gateway_client->ssl_handle = SSL_new(gateway_client->server->ssl_ctx);
    gateway_client->read_bio = BIO_new(BIO_s_mem());
    gateway_client->write_bio = BIO_new(BIO_s_mem());
    SSL_set_bio(gateway_client->ssl_handle, gateway_client->read_bio, gateway_client->write_bio);
    gateway_client->tcp_handle = tcp_handle;
    uv_tcp_init(gateway_client->server->loop, tcp_handle);
    tcp_handle->data = gateway_client;
    if (uv_accept(server, (uv_stream_t*)tcp_handle) == 0) {
        int r = uv_read_start((uv_stream_t *) tcp_handle, AllocateBuffer, on_read);
        if(r == -1) {
            printf("ERROR: uv_read_start error: %s\n", uv_strerror(r));
            ::exit(0);
        }
    }
    else {
        uv_close((uv_handle_t*)tcp_handle, NULL);
    }
}


void HandleSignal(uv_signal_t *signal, int signum) {
    uv_loop_close(signal->loop);
}

void OnInterval(uv_timer_t *handle) {
    auto http_server = static_cast<HttpServer*>(handle->data);

    // If parent is dead(parent id is changed), kill the server.
    if (getppid() != http_server->parent_pid) {
        http_server->Close();
    }
}

HttpServer::HttpServer(uv_loop_t* loop)
    : loop(loop) {
}

void HttpServer::Listen(const char *host, unsigned int port) {
    SSL_library_init();
    SSL_load_error_strings();

    parent_pid = getppid();
    GatewayClient* client = new GatewayClient;
    client->server = this;
    SetSecurityContext();
    memory_pool = new MemoryPool(1024 * 4 * 10000, 1024 * 4);

    uv_signal_t* signal = (uv_signal_t*)malloc(sizeof(uv_signal_t));
    uv_signal_init(loop, signal);
    uv_signal_start(signal, HandleSignal, SIGTERM);
    uv_signal_start(signal, HandleSignal, SIGINT);
    uv_signal_start(signal, HandleSignal, SIGHUP);
    uv_timer_t* timer_request = (uv_timer_t*)malloc(sizeof(uv_timer_t));
    uv_timer_init(loop, timer_request);
    timer_request->data = this;
    uv_timer_start(timer_request, OnInterval, 0, 0);
    uv_tcp_t* server = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, server);
    server->data = client;
    sockaddr_in addr;
    uv_ip4_addr(host, port, &addr);
    uv_tcp_bind(server, (sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t *) server, 128, OnNewConnection);
    if (r) {
        std::fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    }
}

void HttpServer::Close() {
    uv_loop_close(loop);
}

void HttpServer::SetSecurityContext() {
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