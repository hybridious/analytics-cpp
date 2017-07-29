//
// Copyright 2017 Segment Inc. <friends@segment.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#ifdef _WIN32
#include "http-wininet.hpp"

#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>

#include <windows.h>
#include <wininet.h>

namespace segment {
namespace http {

    // This isn't all possible error values, but it does give the
    // majority of them.  This helps when FormatMessage fails.
    struct {
        int code;
        const char* msg;
    } WinInetErrors[] = {
        { ERROR_INTERNET_OUT_OF_HANDLES, "Out of handles" },
        { ERROR_INTERNET_TIMEOUT, "Request timed out" },
        { ERROR_INTERNET_EXTENDED_ERROR, "Extended error" },
        { ERROR_INTERNET_INTERNAL_ERROR, "Internal error" },
        { ERROR_INTERNET_INVALID_URL, "Invalid URL" },
        { ERROR_INTERNET_UNRECOGNIZED_SCHEME, "Invalid scheme" },
        { ERROR_INTERNET_NAME_NOT_RESOLVED, "Host not found" },
        { ERROR_INTERNET_PROTOCOL_NOT_FOUND, "Protocol not found" },
        { ERROR_INTERNET_SHUTDOWN, "Network unloaded" },
        { ERROR_INTERNET_INVALID_OPERATION, "Invalid operation" },
        { ERROR_INTERNET_INCORRECT_HANDLE_STATE, "Bad handle state" },
        { ERROR_INTERNET_NOT_PROXY_REQUEST, "Not proxy request" },
        { ERROR_INTERNET_NO_DIRECT_ACCESS, "No direct access" },
        { ERROR_INTERNET_INCORRECT_FORMAT, "Incorrect format" },
        { ERROR_INTERNET_CANNOT_CONNECT, "Connection refused" },
        { ERROR_INTERNET_CONNECTION_ABORTED, "Connection aborted" },
        { ERROR_INTERNET_CONNECTION_RESET, "Connection reset" },
        { ERROR_INTERNET_SEC_CERT_DATE_INVALID, "Bad certificate date" },
        { ERROR_INTERNET_SEC_CERT_CN_INVALID, "Bad certificate common name" },
        { ERROR_HTTP_HEADER_NOT_FOUND, "Header not found" },
        { ERROR_HTTP_INVALID_HEADER, "Invalid header" },
        { ERROR_HTTP_REDIRECT_FAILED, "Redirect failed" },
        { 0, NULL },
    };

    std::unique_ptr<Response> HandlerWinInet::Handle(const Request& req)
    {
        URL_COMPONENTS urlComp;
        HINTERNET internet;
        HINTERNET connect;
        HINTERNET request;
        DWORD status = 0;
        char message[256];
        DWORD size;
        DWORD secflag = 0;
        const char* acctypes[] = { "*/*", NULL };
        std::string host;
        std::string path;

        auto resp = std::unique_ptr<Response>(new Response());

        // Extract the hostname form the URL.
        ZeroMemory(&urlComp, sizeof(urlComp));
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.dwHostNameLength = (DWORD)-1;
        urlComp.dwUrlPathLength = (DWORD)-1;

        if (!InternetCrackUrlA(req.URL.c_str(), 0, 0, &urlComp)) {
            goto fail;
        }

        host = std::string(urlComp.lpszHostName, urlComp.dwHostNameLength);
        path = std::string(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
        if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
            secflag = INTERNET_FLAG_SECURE;
        }

        internet = InternetOpenA("SegmentWinInet/0.0",
            INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (internet == NULL) {
            goto fail;
        }

        connect = InternetConnectA(internet, host.c_str(), urlComp.nPort,
            NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (connect == NULL) {
            goto fail;
        }

        request = HttpOpenRequestA(connect, "POST", path.c_str(),
            NULL, NULL, acctypes,
            INTERNET_FLAG_NO_UI | secflag,
            NULL);
        if (request == NULL) {
            goto fail;
        }

        for (auto h : req.Headers) {
            std::string hdr = h.first + ": " + h.second + "\r\n";
            if (!HttpAddRequestHeadersA(request, hdr.c_str(), -1,
                    HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE)) {
                goto fail;
            }
        }
        if (!HttpSendRequestA(request, NULL, -1,
                (void*)req.Body.c_str(), req.Body.length())) {
            goto fail;
        }

        size = sizeof(status);
        if (!HttpQueryInfoA(request,
                HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                (void*)&status, &size, 0)) {
            goto fail;
        }
        size = sizeof(message);
        if (!HttpQueryInfoA(request, HTTP_QUERY_STATUS_TEXT, message,
                &size, 0)) {
            goto fail;
        }

        if (request != NULL) {
            InternetCloseHandle(request);
        }
        if (connect != NULL) {
            InternetCloseHandle(connect);
        }
        if (internet != NULL) {
            InternetCloseHandle(internet);
        }
        resp->Code = status;
        resp->Message = message;
        resp->Body = std::string();
        return (resp);

    fail:
        char errbuf[256];
        int rv = GetLastError();

        resp->Code = rv;
        resp->Body = "";
        resp->Message = "";

        for (int i = 0; WinInetErrors[i].msg != NULL; i++) {
            if (rv == WinInetErrors[i].code) {
                resp->Message = WinInetErrors[i].msg;
                break;
            }
        }
        if ((resp->Message == "") && FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, rv, 0, errbuf, 256, NULL)) {
            resp->Message = errbuf;
        }
        if (resp->Message == "") {
            resp->Message = "Windows error " + std::to_string(rv);
        }

        if (request != NULL) {
            InternetCloseHandle(request);
        }
        if (connect != NULL) {
            InternetCloseHandle(connect);
        }
        if (internet = NULL) {
            InternetCloseHandle(internet);
        }
        throw std::runtime_error(resp->Message);
        return (resp);
    }

} // namespace http
} // namespace segment

#endif // _WIN32
