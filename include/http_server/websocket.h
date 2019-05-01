#pragma once

#include <string>
#include <string_view>

namespace http_server {

// See https://tools.ietf.org/html/rfc6455#section-11.8
enum class websocket_opcode
{
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CONNECTION_CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xa
};

/// Represents incoming websocket message
class websocket_message
{
public:
    /// \return message data
    virtual std::string_view get_data() const = 0;

    /// \return message opcode
    virtual websocket_opcode get_opcode() const = 0;
};

/// Client handle for websocket connections
///
/// Can be used to retrieve open connections outside of handlers
/// \see http_server::server::get_websocket_connection
using websocket_handle = const void*;

/// Represents open websocket connection
class websocket_connection
{
public:
    /// \return client handle for the connection
    virtual websocket_handle get_handle() const = 0;

    /// send \a data through the connection (using TEXT opcode)
    /// \return number of bytes sent, 0 for closed connection, -1 for error
    /// TODO: check need for send variants
    virtual int send(std::string_view data) = 0;
};

} // namespace http_server
