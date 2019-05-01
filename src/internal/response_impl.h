#pragma once

#include "http_server/response.h"

#include <civetweb.h>

#include <ostream>
#include <sstream>
#include <string>

namespace http_server {
namespace internal {

/// \see http_server::response
class response_impl : public response
{
public:
    response_impl(mg_connection* connection);
    ~response_impl();

    void ignore();

    void set_status(int code, const std::string& text) override;
    std::ostream& out() override;

private:
    mg_connection* connection_;

    struct
    {
        int code;
        std::string text;
    } status_;
    std::ostringstream contents_;

    bool send_;
};

inline response_impl::response_impl(mg_connection* connection)
: connection_(connection), status_{500, "unknown server error"}, contents_(), send_(true)
{
}

inline response_impl::~response_impl()
{
    if (send_)
    {
        mg_printf(
            connection_,
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n",
            status_.code, status_.text.c_str());
        mg_printf(connection_, "%s", contents_.str().c_str());
    }
}

void response_impl::ignore()
{
    send_ = false;
}

inline void response_impl::set_status(int code, const std::string& text)
{
    status_ = {code, text};
}

inline std::ostream& response_impl::out()
{
    return contents_;
}

} // namespace internal
} // namespace http_server
