#pragma once

#include <regex>
#include <string_view>

namespace http_server {

/// Represents an incoming HTTP request
class request
{
public:
    /// \return regex matches on the local uri
    ///
    /// First match is always the whole uri directory part.
    virtual const std::smatch& get_url_matches() const = 0;

    /// \return query string, i.e. everything after '?' in the local uri (excluding the '?')
    virtual std::string_view get_query_string() const = 0;

    /// \return HTTP method (one of GET, POST, DELETE, etc.)
    virtual std::string_view get_method() const = 0;
};

} // namespace http_server
