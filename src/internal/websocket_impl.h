#pragma once

#include "http_server/websocket.h"

#include <civetweb.h>

#include <cassert>
#include <string>
#include <string_view>

namespace http_server {
namespace internal {

/// \return websocket handle corresponding to a civetweb \a connection
static websocket_handle get_websocket_handle(const mg_connection* connection);

/// \see http_server::websocket_message
class websocket_message_impl : public websocket_message
{
public:
    websocket_message_impl(char* data, size_t len, websocket_opcode opcode);

    std::string_view get_data() const override;
    websocket_opcode get_opcode() const override;

private:
    std::string_view data_;
    websocket_opcode opcode_;
};

/// \see http_server::websocket_connection
class websocket_connection_impl : public websocket_connection
{
public:
    websocket_connection_impl(mg_connection* connection);
    websocket_connection_impl(const mg_connection* connection);

    websocket_handle get_handle() const override;
    int send(std::string_view str) override;

private:
    const mg_connection* const_connection_;
    mg_connection* connection_;
};

// websocket_message_impl

websocket_message_impl::websocket_message_impl(char* data, size_t len, websocket_opcode opcode)
: data_(data, len), opcode_(opcode)
{
}

std::string_view websocket_message_impl::get_data() const
{
    return data_;
}

websocket_opcode websocket_message_impl::get_opcode() const
{
    return opcode_;
}

// websocket_connection_impl

websocket_connection_impl::websocket_connection_impl(mg_connection* connection)
: const_connection_(connection), connection_(connection)
{
}

websocket_connection_impl::websocket_connection_impl(const mg_connection* connection)
: const_connection_(connection), connection_(nullptr)
{
}

websocket_handle websocket_connection_impl::get_handle() const
{
    return get_websocket_handle(const_connection_);
}

int websocket_connection_impl::send(std::string_view str)
{
    assert(connection_);
    return mg_websocket_write(connection_, MG_WEBSOCKET_OPCODE_TEXT, str.data(), str.size());
}

inline static websocket_handle get_websocket_handle(const mg_connection* connection)
{
    return static_cast<websocket_handle>(connection);
}

} // namespace internal
} // namespace http_server
