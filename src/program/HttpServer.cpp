#include <program/HttpServer.h>
#include <program/HttpParser.h>
#include <program/HttpWriter.h>
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
#include "HttpException.h"

#define HTTP_WRITE(string) \
    string, sizeof(string)

using namespace boost::filesystem;

namespace flashpoint {

void
ForwardRequestToGraphQlEndpoint(
    GraphQlFieldRequest *graphql_field_request,
    Field *field);

ExecutableDefinition*
ParseGraphQlRequest(GatewayClient *gateway_client);

void
AllocateBuffer(
    uv_handle_t *handle,
    size_t suggested_size,
    uv_buf_t *buf)
{
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}

void
OnCloseGraphQlFieldRequest(uv_handle_t *handle)
{
    printf("Closed GraphQL field request.\n");
}

void
OnCloseGatewayClientRequest(uv_handle_t *handle)
{
    printf("Closed GraphQL field request.\n");
}

void
OnWriteEnd(uv_write_t *write_request, int status)
{
    if (status < 0) {
        std::cerr << uv_err_name(status) << std::endl;
    }
    delete write_request;
}

void
WriteToSocket(GatewayClient *client, char *buf, size_t len)
{
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

void
FlushWriteBio(GatewayClient *client)
{
    char buf[1024 * 16];
    int bytes_read = 0;
    while ((bytes_read = BIO_read(client->ssl_handle->wbio, buf, sizeof(buf))) > 0) {
        WriteToSocket(client, buf, bytes_read);
    }
}

void
PrintError(GatewayClient *client, int status)
{
    printf("ERROR: %s\n", uv_strerror(status));
}

void
OnResolvedAllGraphQlFields(GatewayClient *gateway_client)
{
    HttpWriter http_writer(gateway_client->tcp_handle, gateway_client->ssl_handle);
    http_writer.WriteLine("HTTP/1.1 200 OK");
    HttpParser http_parser;
    for (auto field : gateway_client->fields_result) {
        http_parser.ParseResponse(field.second);

        http_writer.WriteLine();
    }
    gateway_client->finished = true;
    uv_close((uv_handle_t*)gateway_client->tcp_handle, OnCloseGraphQlFieldRequest);
}

void
OnGraphQlRequestRead(uv_stream_t *tcp, ssize_t length, const uv_buf_t *buf)
{
    auto field_request = static_cast<GraphQlFieldRequest*>(tcp->data);
    auto gateway_client = field_request->gateway_client;

    if (length >= 0) {
        auto field_it = gateway_client->fields_result.find(field_request->field);
        if (field_it == gateway_client->fields_result.end()) {
            gateway_client->fields_result.emplace(field_request->field, std::vector { new TextSpan(buf->base, length) });
        }
        else {
            field_it->second.push_back(new TextSpan(buf->base, length));
        }
    }
    else {
        gateway_client->resolved_fields++;
        if (gateway_client->resolved_fields == gateway_client->fields_to_resolve) {
            OnResolvedAllGraphQlFields(gateway_client);
        }
        uv_close((uv_handle_t*)tcp, OnCloseGraphQlFieldRequest);
    }
}

void
OnClientConnect(uv_connect_t *connection, int status)
{
    if (status == -1) {
        std::cerr << "error on write_end" << std::endl;
    }
    auto client_request = static_cast<GraphQlFieldRequest*>(connection->data);
    auto gateway_client = client_request->gateway_client;
    uv_stream_t *stream = connection->handle;
    stream->data = connection->data;
    uv_read_start(stream, AllocateBuffer, OnGraphQlRequestRead);
    for (const auto& field : gateway_client->fields) {
        auto http_writer = HttpWriter(client_request->tcp_handle, nullptr);
        http_writer.WriteRequest(HttpMethod::Post, client_request->path);
        auto field_name = field.second->name->identifier;
        http_writer.WriteLine("Host: ", client_request->host);
        http_writer.WriteLine("User-Agent: flash");
        http_writer.WriteLine("Accept: */*");
        http_writer.WriteLine("Content-Type: application/json; charset=utf-8");
        http_writer.ScheduleBodyWrite("{ \"query\": \"{ ");
        http_writer.ScheduleBodyWrite(field.second->name->identifier.c_str());
        http_writer.ScheduleBodyWrite(" }\" }");
        http_writer.CommitBodyWrite();
        http_writer.End();
    }
}

void
OnResolvedIpv4(uv_getaddrinfo_t *addr_request, int status, struct addrinfo *res)
{
    char ip_addr[17] = { '\0' };
    int r = uv_ip4_name((sockaddr_in*)(res->ai_addr), ip_addr, 16);
    if (r) {
        printf("Error getting ipv4 address: %s.\n", uv_strerror(r));
        return;
    }
    auto request_address = (sockaddr_in*)malloc(sizeof(sockaddr_in));
    auto field_request = (GraphQlFieldRequest*)addr_request->data;
    r = uv_ip4_addr(ip_addr, field_request->port, request_address);
    if (r) {
        printf("Error getting ipv4 address: %s.\n", uv_strerror(r));
        return;
    }
    auto connect_request = (uv_connect_t*)malloc(sizeof(uv_connect_t));
    connect_request->data = addr_request->data;
    field_request->tcp_handle = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(addr_request->loop, field_request->tcp_handle);
    uv_tcp_keepalive(field_request->tcp_handle, 1, 60);
    r = uv_tcp_connect(connect_request, field_request->tcp_handle, (sockaddr*)request_address, OnClientConnect);
    if (r) {
        printf("Could not connect to client: %s.\n", uv_strerror(r));
        return;
    }

    uv_freeaddrinfo(res);
}


void
ForwardRequestToGraphQlEndpoint(GraphQlFieldRequest *graphql_field_request, Field *field)
{
    addrinfo* hints = (addrinfo*)malloc(sizeof(addrinfo));
    hints->ai_family = PF_INET;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_protocol = IPPROTO_TCP;
    hints->ai_flags = 0;

    auto addrinfo = (uv_getaddrinfo_t*)malloc(sizeof(uv_getaddrinfo_t));
    addrinfo->data = graphql_field_request;
    auto gateway_client = graphql_field_request->gateway_client;
    auto loop = gateway_client->server->loop;
    int r = uv_getaddrinfo(loop, addrinfo, OnResolvedIpv4, graphql_field_request->hostname, "80", hints);
    if (r) {
        printf("Error at DNS request: %s.\n", uv_strerror(r));
        uv_close((uv_handle_t*)gateway_client->tcp_handle, nullptr);
        return;
    }
}

std::map<const char*, BackendEndpoint, CompareStrings> field_to_endpoint = {
    { "field", { "http://localhost:4000/graphql", "localhost:4000", "localhost", 4000, "/graphql"} }
};

bool
ExecuteGraphQlQuery(GatewayClient *gateway_client)
{
    auto executable_definition = ParseGraphQlRequest(gateway_client);
    if (executable_definition == nullptr) {
        return false;
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
            return false;
        }
        operation_definition = *operation_definition_it;
    }
    for (const auto& selection : operation_definition->selection_set->selections) {
        if (selection->kind != SyntaxKind::S_Field) {
            throw InvalidGraphQlQueryException();
        }
        auto field = static_cast<Field*>(selection);
        auto name = static_cast<Field*>(selection)->name->identifier;
        auto endpoint_it = field_to_endpoint.find(name.c_str());
        if (endpoint_it != field_to_endpoint.end()) {
            auto backend_endpoint = endpoint_it->second;
            gateway_client->fields.emplace(endpoint_it->first, field);
            gateway_client->fragments = &executable_definition->fragment_definitions;
            auto client_request = new GraphQlFieldRequest {
                endpoint_it->first,
                backend_endpoint.hostname,
                backend_endpoint.port,
                backend_endpoint.host,
                backend_endpoint.path,
                nullptr,
                gateway_client,
            };
            gateway_client->fields_to_resolve++;
            ForwardRequestToGraphQlEndpoint(client_request, field);
        }
    }
    return true;
}

void
OnGatewayClientRequestRead(uv_stream_t *client_stream, ssize_t read_length, const uv_buf_t *buf)
{
    if (read_length == 0) {
        free(buf->base);
        uv_close((uv_handle_t*)client_stream, OnCloseGatewayClientRequest);
        return;
    }
    auto gateway_client = static_cast<GatewayClient*>(client_stream->data);
    if (!SSL_is_init_finished(gateway_client->ssl_handle)) {
        BIO_write(gateway_client->ssl_handle->rbio, buf->base, read_length);
        SSL_accept(gateway_client->ssl_handle);
        FlushWriteBio(gateway_client);
        return;
    }
    std::size_t read_buffer_size = 4096;
    char *read_buffer = (char*)gateway_client->server->memory_pool->Allocate(read_buffer_size, 1, gateway_client->ticket);
    BIO_write(gateway_client->ssl_handle->rbio, buf->base, read_length);
    int read_size = SSL_read(gateway_client->ssl_handle, read_buffer, read_buffer_size);
    if (read_size <= 0) {
        // TODO: Handle SSL read error
        SSL_get_error(gateway_client->ssl_handle, read_size);
        PrintError(gateway_client, read_size);
        return;
    }
    else if (read_size > 0) {
        auto http_parser = gateway_client->http_parser;
        http_parser->ParseRequest(new TextSpan(read_buffer, read_size));
        if (http_parser->IsFinished()) {
            ExecuteGraphQlQuery(gateway_client);
        }
    }
}

const char*
ToConstChar(std::vector<TextSpan*>& token_values)
{
    std::size_t size = 0;
    for (const auto& token_value : token_values) {
        size += token_value->length;
    }
    char* text = new char[size];
    for (const auto& token_value : token_values) {
        for (std::size_t i = 0; i < token_value->length; i++) {
            text[i] = token_value->value[i];
        }
    }
    return text;
}

ExecutableDefinition*
ParseGraphQlRequest(GatewayClient *gateway_client)
{
    auto body = gateway_client->http_parser->body;
    if (!body.empty()) {
        Json::Reader json_reader;
        Json::Value request_body;
        json_reader.parse(ToConstChar(body), request_body);
        auto memory_pool = gateway_client->server->memory_pool;
        GraphQlSchema schema("type Query { field: Int }", memory_pool, gateway_client->ticket);
        GraphQlExecutor graphql_executor(memory_pool, gateway_client->ticket);
        graphql_executor.AddSchema(schema);
        std::string graphql_query = request_body["query"].asString();
        return graphql_executor.Execute(graphql_query);
    }
    return nullptr;
}

void OnNewConnection(uv_stream_t *server, int status) {
    if (status < 0) {
        std::fprintf(stderr, "New connection error %s\n", uv_strerror(status));
    }
    auto tcp_handle = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    auto http_server = static_cast<HttpServer*>(server->data);
    GatewayClient* gateway_client = new GatewayClient {
        tcp_handle,
        http_server,
        SSL_new(http_server->ssl_ctx),
        BIO_new(BIO_s_mem()),
        BIO_new(BIO_s_mem()),
        new HttpParser(),
        http_server->memory_pool->TakeTicket(),
    };
    SSL_set_bio(gateway_client->ssl_handle, gateway_client->read_bio, gateway_client->write_bio);
    uv_tcp_init(server->loop, tcp_handle);
    tcp_handle->data = gateway_client;
    if (uv_accept(server, (uv_stream_t*)tcp_handle) == 0) {
        int r = uv_read_start((uv_stream_t *) tcp_handle, AllocateBuffer, OnGatewayClientRequestRead);
        if(r == -1) {
            printf("ERROR: uv_read_start error: %s\n", uv_strerror(r));
            exit(0);
        }
    }
    else {
        uv_close((uv_handle_t*)tcp_handle, NULL);
    }
}


void
HandleSignal(uv_signal_t *signal, int signum) {
    uv_loop_close(signal->loop);
}

HttpServer::HttpServer(uv_loop_t* loop)
    : loop(loop), fd(0) {
}

static void
ReadStdin(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    if (nread <= 0) {
        std::cout << "Closing" << std::endl;
        auto http_server = static_cast<HttpServer*>(stream->data);
        http_server->Close();
        exit(0);
    }
}

void HttpServer::Listen(const char *host, unsigned int port) {
    SSL_library_init();
    SSL_load_error_strings();

    parent_pid = getppid();
    SetSecurityContext();
    memory_pool = new MemoryPool(1024 * 4 * 10000, 1024 * 4);

    uv_signal_t* signal = (uv_signal_t*)malloc(sizeof(uv_signal_t));
    uv_signal_init(loop, signal);
    uv_signal_start(signal, HandleSignal, SIGTERM);
    uv_signal_start(signal, HandleSignal, SIGINT);
    uv_signal_start(signal, HandleSignal, SIGHUP);

    if (fd != 0) {
        uv_tty_t* input = (uv_tty_t*)malloc(sizeof(uv_tty_t));
        input->data = this;
        uv_tty_init(loop, input, fd, 1);
        uv_read_start((uv_stream_t *) input, AllocateBuffer, ReadStdin);
    }

    uv_tcp_t* server = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
    uv_tcp_init(loop, server);
    server->data = this;
    sockaddr_in addr;
    uv_ip4_addr(host, port, &addr);
    uv_tcp_bind(server, (sockaddr*)&addr, 0);
    int r = uv_listen((uv_stream_t*)server, 128, OnNewConnection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    }
}

void
HttpServer::Close()
{
    uv_loop_close(loop);
}

void
HttpServer::SetSecurityContext()
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