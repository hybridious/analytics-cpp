//
// Copyright 2017 Segment Inc. <friends@segment.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "http-curl.hpp"

#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#include <curl/curl.h>

namespace segment {
namespace http {

    // Internal curl exception error.  Most of these will be ENOMEM.
    class curlExcept : public std::exception {
    public:
        curlExcept(CURLcode rv)
        {
            this->code = rv;
        };
        const char* what() const throw()
        {
            return (curl_easy_strerror(this->code));
        }
        CURLcode code;
    };

    class curlReq {
    public:
        curlReq()
        {
            this->headers = NULL;
            this->req = NULL;
        }
        ~curlReq()
        {
            if (this->headers != NULL) {
                curl_slist_free_all(this->headers);
            }
            if (this->req != NULL) {
                curl_easy_cleanup(this->req);
            }
        }

        void addHeader(const std::string key, const std::string value)
        {
            struct curl_slist* hdrs;
            std::string line;

            line = key + ": " + value;
            hdrs = curl_slist_append(this->headers, line.c_str());
            if (hdrs == NULL) {
                throw std::bad_alloc(); // cannot think of any other reason...
            }
            this->headers = hdrs;
        }

        void setBody(const std::string& body)
        {
            this->body = body;
        }

        static size_t writeCallback(char* ptr, size_t sz, size_t nmemb, void* udata)
        {
            curlReq* creq = (curlReq*)udata;
            char* ucdata = (char*)udata;
            size_t nbytes = sz * nmemb;
            creq->respData += std::string(ucdata, nbytes);
            return (nbytes);
        }

        void perform(const std::string meth, const std::string url)
        {
            CURL* req;
            CURLcode rv;
            long code;

            if (meth != "POST") {
                throw std::invalid_argument("Only POST supported");
            }

            if (this->req == NULL) {
                this->req = curl_easy_init();
            }
            if ((req = this->req) == NULL) {
                throw std::bad_alloc();
            }

#define setopt(k, v)                                        \
    if ((rv = curl_easy_setopt(req, k, v)) != CURLE_OK) {   \
        if (rv == CURLE_OUT_OF_MEMORY) {                    \
            throw std::bad_alloc();                         \
        } else {                                            \
            throw std::invalid_argument("bad curl option"); \
        }                                                   \
    }

            std::string body(this->body.begin(), this->body.end());

            // We only handle post.
            setopt(CURLOPT_URL, url.c_str());
            setopt(CURLOPT_POSTFIELDS, body.c_str());
            setopt(CURLOPT_POSTFIELDSIZE, body.length());
            setopt(CURLOPT_POST, 1);
            setopt(CURLOPT_HTTPHEADER, this->headers);
            setopt(CURLOPT_WRITEFUNCTION, this->writeCallback);
            setopt(CURLOPT_WRITEDATA, this);
            //            setopt(CURLOPT_VERBOSE, 1);
            //            printf("POSTING: %s\n", body.c_str());

            if ((rv = curl_easy_perform(req)) != CURLE_OK) {
                if (rv == CURLE_OUT_OF_MEMORY) {
                    throw std::bad_alloc();
                }
            }

#define getinfo(k, v)                                      \
    if ((rv = curl_easy_getinfo(req, k, v)) != CURLE_OK) { \
        if (rv == CURLE_OUT_OF_MEMORY) {                   \
            throw std::bad_alloc();                        \
        }                                                  \
    }

            // get status
            getinfo(CURLINFO_RESPONSE_CODE, &code);
            if (code == 0) {
                long errn;
                getinfo(CURLINFO_OS_ERRNO, &errn);
                throw std::system_error((int)errn, std::system_category());
            }
            if (code > 299) {
                throw Error(code);
            }
            this->respCode = (int)code;
        }

        // This is probably not terribly idiomatic C++, but we put them
        // here to make things easy.  C++ class friends would be better,
        // but we're leaving this here in the implementation, so it should
        // be fine.  These are used by the HttpHandlerCurl class.

        std::string respData;
        std::string respMessage;
        int respCode;

    private:
        struct curl_slist* headers;
        std::string body;
        CURL* req;
    };

    std::unique_ptr<Response> HandlerCurl::Handle(const Request& req)
    {

        auto resp = std::unique_ptr<Response>(new Response());
        curlReq creq;

        for (auto const& item : req.Headers) {
            creq.addHeader(item.first, item.second);
        }

        creq.setBody(req.Body);
        creq.perform("POST", req.URL);
        resp->Code = creq.respCode;
        resp->Message = creq.respMessage;
        resp->Body = creq.respData;

        return resp;
    }

} // namespace http
} // namespace segment
