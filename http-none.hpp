//
// Copyright 2017 Segment Inc. <friends@segment.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#ifndef SEGMENT_HTTP_NONE_HPP_
#define SEGMENT_HTTP_NONE_HPP_

#include "http.hpp"

// This is a dumb-do-nothing implementation of the HttpHandler.
// It is intended to stand-in for a default, if no better
// implementation is available.

namespace segment {
namespace http {

    /// HttpHandlerNone is a stub implementation that does not actually
    /// do anything.  It exists to be compiled into binaries that don't
    /// use libcurl, so that a NOP default can exist.
    class HandlerNone : public Handler {
    public:
        std::unique_ptr<Response> Handle(const Request&)
        {
            auto resp = std::unique_ptr<Response>(new Response());
            resp->Code = 0;
            resp->Message = "Unimplemented.";
            return resp;
        };
    };

} // namespace http
} // namespace segment

#endif // SEGMENT_HTTP_NONE_HPP_