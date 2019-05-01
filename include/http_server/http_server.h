#pragma once

#include "http_server/request.h"
#include "http_server/response.h"
#include "http_server/websocket.h"
#include <functional>
#include <memory>

namespace http_server {

/// HTTP server
///
///
class server
{
public:
    // TODO: server configuration, e.g. listening port
    server();
    ~server();

    /// Signature for request handler functions.
    ///
    /// \param req [in] incoming request
    /// \param res [out] response to the request
    /// \return true if the request was fully handled
    using handler_func = std::function<bool(const request& req, response& res)>;

    /// Add handler \a func for any HTTP request to url matching (regex) string \a uri_matcher
    void add_handler(const std::string& uri_matcher, const handler_func& func);

    /// Add handler \a func for HTTP request to method matching (regex) string \a method_matcher and
    /// url matching (regex) string \a uri_matcher
    void add_handler(
        const std::string& method_matcher,
        const std::string& uri_matcher,
        const handler_func& func);

    /// \defgroup Signatures for websocket handler functions.
    ///
    /// Connection, receiving data and disconnection.
    ///
    /// \param connection [in] websocket connection under event
    /// \param message [in] message received through the connection
    ///
    /// @{
    using websocket_connection_handler_func = std::function<void(websocket_connection& connection)>;
    using websocket_data_handler_func =
        std::function<void(websocket_connection& connection, const websocket_message& message)>;
    using websocket_disconnection_func = std::function<void(const websocket_connection& connection)>;
    /// @}

    /// Add handlers for event on websocket opened using url matching (regex) string \a matcher
    ///
    /// \param matcher [in] incoming request
    /// \param connection_func [in] connection handler callback
    /// \param data_func [in] data receive handler callback
    /// \param disconnection_func [in] disconnection handler callback
    void add_websocket_handler(
        const std::string& matcher,
        const websocket_connection_handler_func& connection_func,
        const websocket_data_handler_func& data_func,
        const websocket_disconnection_func& disconnection_func);

    /// \return connection matching client \a handle, or nullptr if no matching connection
    /// TODO: need for shared ptr here?
    websocket_connection* get_websocket_connection(websocket_handle handle);

    /// RAII lock for server context
    class lock
    {
    public:
        lock(server& s);
        ~lock();

    private:
        server& s_;
    };

private:
    void lock_server();
    void unlock_server();

    class impl;
    std::unique_ptr<impl> impl_;
};

// server::lock

inline server::lock::lock(server& s) : s_(s)
{
    s_.lock_server();
}

inline server::lock::~lock()
{
    s_.unlock_server();
}

} // namespace http_server
