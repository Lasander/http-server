#pragma once

#include <ostream>
#include <string>

namespace http_server {

/// Represents the response to an incoming HTTP request
class response
{
public:
    /// Set response status \a code and \a text
    virtual void set_status(int code, const std::string& text) = 0;

    /// \return output stream to write response data to
    ///
    /// Most conveniently used through the free stream operator by:
    ///   response << "my response data";
    virtual std::ostream& out() = 0;
};

/// Convenience helper to write response data directly using the response object
/// \see http_server::response::out
template<typename T>
inline std::ostream& operator<<(response& res, const T& data)
{
    return res.out() << data;
}

} // namespace http_server
