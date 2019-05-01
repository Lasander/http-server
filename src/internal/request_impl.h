#pragma once

#include "http_server/request.h"

#include <civetweb.h>

#include <cassert>

namespace http_server {
namespace internal {

/// \see http_server::request
class request_impl : public request
{
public:
    request_impl(const std::smatch& url_matches, const mg_request_info& info);

    const std::smatch& get_url_matches() const override;
    std::string_view get_query_string() const override;
    std::string_view get_method() const override;

private:
    const std::smatch& url_matches_;
    const mg_request_info& info_;
};

inline request_impl::request_impl(const std::smatch& url_matches, const mg_request_info& info)
: url_matches_(url_matches), info_(info)
{
}

inline const std::smatch& request_impl::get_url_matches() const
{
    return url_matches_;
}

inline std::string_view request_impl::get_query_string() const
{
    return info_.query_string ? info_.query_string : std::string_view();
}

inline std::string_view request_impl::get_method() const
{
    assert(info_.request_method);
    return info_.request_method;
}

} // namespace internal
} // namespace http_server
