//
// Copyright 2017 Segment Inc. <friends@segment.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include <exception>
#include <map>
#include <memory>
#include <string>

#ifndef SEGMENT_HTTP_HPP_
#define SEGMENT_HTTP_HPP_

namespace segment {
namespace http {

    /// Error models an error or exception from the HTTP implementation.
    /// Implementations may throw this instead of returning an error inline.
    class Error : public std::exception {

    public:
        /// Constructor.
        /// @param code [in] The HTTP result code.  This should be a
        ///                  valid HTTP status code, unless the request was
        ///                  not serviced at all, in which case it should be zero.
        Error(int code)
        {
            this->code = code;
            this->msg = "HTTP Error " + std::to_string(code);
        }

        /// Another constructor.
        /// @param code [in] The HTTP result code.  This should be a
        ///                  valid HTTP status code, unless the request was
        ///                  not serviced at all, in which case it should be zero.
        /// @param msg [in] Override the default message.
        Error(int code, std::string msg)
        {
            this->code = code;
            this->msg = msg;
        }

        /// what returns the the message, and is used in the exception.
        const char* what() const throw() { return msg.c_str(); }

        /// code is the atual HTTP result code, or 0.
        int code;

        /// msg is the message returned by what(), a human explanation of the
        /// error.
        std::string msg;
    };

    /// Request models an HTTP request, such as a POST.  In fact, as of
    /// this writing, only POST is supported, as it is all that the existing
    /// analytics framework requires.
    class Request {
    public:
        /// Method should be GET, POST, or similar.  At present only POST is
        /// supported.  This should match *exactly* what the HTTP standard
        /// requires (all capitals, no whitespace, etc.)
        std::string Method;

        /// URL represents the entire URL, including the Host and the Path.
        std::string URL;

        /// Note that if you want to have multiple values for the same map key,
        /// you have to use comma seperated white space values.  The HTTP RFCs
        /// insist that this must be permitted, and must not change any
        /// semantics.  Demanding that our consumers use this semantic means
        /// that we can give them a nice familiar API (STL maps).
        std::map<std::string, std::string> Headers;

        /// The request body is here.  Normally this will be something like
        /// serialized JSON.  This will generally be reasonably small, and
        /// never larger than about 512k as Segment prohibits uploading
        /// more data than that in a single POST.
        std::string Body;
    };

    /// HttpResponse models a result from performing an HTTP request.
    class Response {
    public:
        /// Code is an HTTP response code, like 200 or 404.  If the request
        /// could not be delivered to the server, or a valid response could
        /// not be obtained, then the code 0 should be used, and the Message
        /// below should be completed with an error message.
        int Code;

        /// Message is a message explaining the result.  Normally this comes
        /// directly from the HTTP server, but it can also be a system
        /// error message (e.g. "Host not found" or "No route to destination" or
        /// even "Out of memory").
        std::string Message;

        /// Headers are the response headers we got back from the server.
        /// Nothing in the current code actually cares about this.  The
        /// implementation may not actually fill this in as a result.
        std::map<std::string, std::string> Headers;

        /// Body is the response body, if a payload was returned.
        /// Nothing actually uses this at present, and at present the
        /// Segment API returns an empty body anyway.
        std::string Body;
    };

    /// HttpHandler is an abstract class for handling HTTP.  Note that
    /// at present we only strictly require the ability to POST, and we
    /// don't really care if the implementation bothers to flesh out the
    /// Body of the response.  Nonetheless, it is recommended that
    /// implementations do fill this out to future-proof in case we
    /// begin using more fully featured handles.
    class Handler {
    public:
        virtual ~Handler(){};
        /// Handle taks a request and turns it into a response, generally
        /// by posting it to the Segment API service.  This API is strictly
        /// synchronous, but Segment always performs this operation from
        /// a context where this is safe.
        /// @param [in] req The HTTP request object.
        /// @return The HTTP response object.
        virtual std::unique_ptr<Response> Handle(const Request& req) = 0;
    };

} // namespace http
} // namespace segment

#endif // SEGMENT_HTTP_HPP