#include "http_server/http_server.h"
#include "internal/request_impl.h"
#include "internal/response_impl.h"
#include "internal/websocket_impl.h"

#include <civetweb.h>

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std::string_literals;
using namespace http_server::internal;

namespace http_server {
namespace {

/// RAII lock for civerweb connection object
class connection_lock
{
public:
    connection_lock(mg_connection* conn) : conn_(conn)
    {
        mg_lock_connection(conn_);
    }
    ~connection_lock()
    {
        mg_unlock_connection(conn_);
    }

private:
    mg_connection* conn_;
};

} // anonymous namespace

/// \see http_server::server
class server::impl
{
public:
    impl();
    ~impl();

    void add_handler(
        const std::string& method_matcher,
        const std::string& uri_matcher,
        const handler_func& func);

    void add_websocket_handler(
        const std::string& matcher,
        const websocket_connection_handler_func& connection_func,
        const websocket_data_handler_func& data_func,
        const websocket_disconnection_func& disconnection_func);

    websocket_connection* get_websocket_connection(websocket_handle handle);

    void lock_server();
    void unlock_server();

private:
    // Dispatching handler for all incoming http requests
    static int dispatch_request(mg_connection* conn, void* cbdata);

    // Handlers for all incoming websocket events
    static int websocket_connect_handler(const mg_connection* conn, void* cbdata);
    static void websocket_ready_handler(mg_connection* conn, void* cbdata);
    static int websocket_data_handler(mg_connection* conn, int flags, char* data, size_t len, void* cbdata);
    static void websocket_close_handler(const mg_connection* conn, void* cbdata);

    mg_context* ctx_;

    // HTTP request handler record
    struct handler
    {
        std::regex method_matcher;
        std::regex uri_matcher;
        handler_func func;
    };
    // active HTTP request handlers
    std::vector<handler> handlers_;

    // websocekt handler record
    struct ws_handler
    {
        std::regex matcher;
        websocket_connection_handler_func connection_func;
        websocket_data_handler_func data_func;
        websocket_disconnection_func disconnection_func;
    };
    // active websocket handlers
    std::vector<ws_handler> ws_handlers_;

    // Helper to tie together the websocket handler and the
    // associated civetweb connection
    //
    // There can be multiple connection per handler
    class ws_client
    {
    public:
        ws_client(mg_connection* conn, ws_handler& handler);
        ~ws_client();

        ws_client(const ws_client&) = delete;
        ws_client& operator=(const ws_client&) = delete;

        websocket_connection& get_connection();
        ws_handler& get_handler();
        bool is_ready() const;
        void set_ready();

        static ws_client& get_client(const mg_connection* conn);

    private:
        mg_connection* conn_;
        websocket_connection_impl connection_;
        ws_handler& handler_;
        bool is_ready_;
    };
    // active websocket connections
    std::unordered_map<websocket_handle, ws_client> ws_clients_;
};

// server::impl

server::impl::impl() : handlers_(), ws_handlers_(), ws_clients_()
{
    const char* options[] = {
        "document_root", ".", "listening_ports", "8080", "websocket_timeout_ms", "3600000", 0};

    // struct mg_callbacks callbacks;
    // memset(&callbacks, 0, sizeof(callbacks));
    // callbacks.log_message = log_message;

    ctx_ = mg_start(nullptr, 0, options);
    if (ctx_ == nullptr)
    {
        fprintf(stderr, "Cannot start server - mg_start failed.\n");
    }

    if (!mg_check_feature(16))
    {
        std::cerr << "Error: Embedded example built with websocket support, "
                  << "but civetweb library build without." << std::endl;
    }

    // Handler for all requests
    mg_set_request_handler(ctx_, "/", dispatch_request, this);
    // Handler for all websocket traffic
    mg_set_websocket_handler(
        ctx_, "/", websocket_connect_handler, websocket_ready_handler, websocket_data_handler,
        websocket_close_handler, this);
}

server::impl::~impl()
{
    mg_stop(ctx_);
}

void server::impl::add_handler(
    const std::string& method_matcher,
    const std::string& uri_matcher,
    const handler_func& func)
{
    std::cout << "add_handler: " << method_matcher << " - " << uri_matcher << std::endl;
    handler h = {std::regex(method_matcher, std::regex::icase), std::regex(uri_matcher), func};
    handlers_.push_back(h);
}

void server::impl::add_websocket_handler(
    const std::string& matcher,
    const websocket_connection_handler_func& connection_func,
    const websocket_data_handler_func& data_func,
    const websocket_disconnection_func& disconnection_func)
{
    std::cout << "add_websocket_handler: " << matcher << std::endl;
    ws_handler h = {std::regex(matcher), connection_func, data_func, disconnection_func};
    ws_handlers_.push_back(h);
}

websocket_connection* server::impl::get_websocket_connection(websocket_handle handle)
{
    auto it = ws_clients_.find(handle);
    if (it != ws_clients_.end())
    {
        return &it->second.get_connection();
    }
    return nullptr;
}

void server::impl::lock_server()
{
    mg_lock_context(ctx_);
}

void server::impl::unlock_server()
{
    mg_unlock_context(ctx_);
}

int server::impl::dispatch_request(mg_connection* conn, void* cbdata)
{
    const mg_request_info* req = mg_get_request_info(conn);
    assert(req);
    std::string method(req->request_method);
    std::string local_uri(req->local_uri);

    impl* s = static_cast<impl*>(cbdata);
    for (auto& h : s->handlers_)
    {
        if (!std::regex_match(method, h.method_matcher))
        {
            continue;
        }

        std::smatch match;
        if (std::regex_match(local_uri, match, h.uri_matcher))
        {
            response_impl response(conn);
            if (!(h.func)(request_impl(match, *req), response))
            {
                response.ignore();
                return 0;
            }
            return 1;
        }
    }

    response_impl defaut_response(conn);
    defaut_response.set_status(404, "Not found"s);
    defaut_response << "<html><body>"
                    << "<h2>Page not found!</h2>"
                    << "</body></html>\n";
    return 1;
}

int server::impl::websocket_connect_handler(const mg_connection* conn, void* cbdata)
{
    const mg_request_info* req = mg_get_request_info(conn);
    std::string local_uri(req->local_uri);

    impl* s = static_cast<impl*>(cbdata);
    for (auto& h : s->ws_handlers_)
    {
        std::smatch match;
        if (std::regex_match(local_uri, match, h.matcher))
        {
            s->lock_server();
            s->ws_clients_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(get_websocket_handle(conn)),
                std::forward_as_tuple(const_cast<mg_connection*>(conn), h));
           s->unlock_server();
            return 0;
        }
    }

    // TODO: accoding to
    // https://github.com/civetweb/civetweb/blob/master/docs/api/mg_set_user_connection_data.md
    // we should not use mg_write or mg_read in this handler.
    // However, we should be allowed to respond with a reasonable code, right?
    // The below code looks to work.

    // mg_printf(const_cast<mg_connection*>(conn),
    //     "HTTP/1.1 405 WebSocket connection not allowed\r\nContent-Type: "
    //     "text/html\r\nConnection: "
    //     "close\r\n\r\n");

    // Reject (don't process) connection
    return 1;
}

void server::impl::websocket_ready_handler(mg_connection* conn, void* cbdata)
{
    ws_client& client = ws_client::get_client(conn);
    assert(!client.is_ready());

    websocket_connection_impl connection(conn);
    {
        connection_lock lk(conn);
        client.get_handler().connection_func(connection);
    }

    client.set_ready();
}

int server::impl::websocket_data_handler(mg_connection* conn, int flags, char* data, size_t len, void* cbdata)
{
    ws_client& client = ws_client::get_client(conn);
    assert(client.is_ready());

    const auto opcode = static_cast<websocket_opcode>(flags & 0x0F);
    websocket_connection_impl connection(conn);
    {
        connection_lock lk(conn);
        client.get_handler().data_func(connection, websocket_message_impl(data, len, opcode));
    }

    return 1;
}

void server::impl::websocket_close_handler(const mg_connection* conn, void* cbdata)
{
    ws_client& client = ws_client::get_client(conn);
    assert(client.is_ready());

    websocket_connection_impl connection(conn);
    client.get_handler().disconnection_func(connection);

    impl* s = static_cast<impl*>(cbdata);

    mg_lock_context(s->ctx_);

    auto erased = s->ws_clients_.erase(get_websocket_handle(conn));
    assert(erased == 1);

    mg_unlock_context(s->ctx_);
}

// server::impl::ws_client

server::impl::ws_client::ws_client(mg_connection* conn, ws_handler& handler)
: conn_(conn), connection_(conn), handler_(handler), is_ready_(false)
{
    assert(conn_);
    mg_set_user_connection_data(conn_, this);
}

server::impl::ws_client::~ws_client()
{
    mg_set_user_connection_data(conn_, nullptr);
}

websocket_connection& server::impl::ws_client::get_connection()
{
    return connection_;
}

server::impl::ws_handler& server::impl::ws_client::get_handler()
{
    return handler_;
}

bool server::impl::ws_client::is_ready() const
{
    return is_ready_;
}

void server::impl::ws_client::set_ready()
{
    is_ready_ = true;
}

server::impl::ws_client& server::impl::ws_client::get_client(const mg_connection* conn)
{
    ws_client* client = static_cast<ws_client*>(mg_get_user_connection_data(conn));
    assert(client);
    return *client;
}

// server

server::server() : impl_(std::make_unique<impl>())
{
}

server::~server()
{
}

void server::add_handler(const std::string& uri_matcher, const handler_func& func)
{
    impl_->add_handler(".*"s, uri_matcher, func);
}

void server::add_handler(
    const std::string& method_matcher,
    const std::string& uri_matcher,
    const handler_func& func)
{
    impl_->add_handler(method_matcher, uri_matcher, func);
}

void server::add_websocket_handler(
    const std::string& matcher,
    const websocket_connection_handler_func& connection_func,
    const websocket_data_handler_func& data_func,
    const websocket_disconnection_func& disconnection_func)
{
    impl_->add_websocket_handler(matcher, connection_func, data_func, disconnection_func);
}

websocket_connection* server::get_websocket_connection(websocket_handle handle)
{
    return impl_->get_websocket_connection(handle);
}

void server::lock_server()
{
    return impl_->lock_server();
}

void server::unlock_server()
{
    return impl_->unlock_server();
}

} // namespace http_server
