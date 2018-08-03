#include <iostream>
#include "http_scanner.h"
#include "http_response.h"

namespace flashpoint::program {

    std::map<HttpHeader, const char*> token_to_string = {
        { HttpHeader::Accept, "Accept" },
        { HttpHeader::AcceptCharset, "Accept-Charset" },
        { HttpHeader::AcceptEncoding, "Accept-Encoding" },
        { HttpHeader::AcceptLanguage, "Accept-Language" },
        { HttpHeader::AcceptPost, "Accept-Post" },
        { HttpHeader::AcceptRanges, "Accept-Ranges" },
        { HttpHeader::Age, "Age" },
        { HttpHeader::Allow, "Allow" },
        { HttpHeader::ALPN, "ALPN" },
        { HttpHeader::AltSvc, "Alt-Svc" },
        { HttpHeader::AltUsed, "Alt-Used" },
        { HttpHeader::AuthenticationInfo, "Authentication-Info" },
        { HttpHeader::Authorization, "Authorization" },
        { HttpHeader::CacheControl, "Cache-Control" },
        { HttpHeader::CalDAVTimezones, "CalDAV-Timezones" },
        { HttpHeader::Connection, "Connection" },
        { HttpHeader::ContentDisposition, "Content-Disposition" },
        { HttpHeader::ContentEncoding, "Content-Encoding" },
        { HttpHeader::ContentLanguage, "Content-Language" },
        { HttpHeader::ContentLength, "Content-Length" },
        { HttpHeader::ContentLocation, "Content-Location" },
        { HttpHeader::ContentRange, "Content-Range" },
        { HttpHeader::ContentType, "Content-Type" },
        { HttpHeader::Cookie, "Cookie" },
        { HttpHeader::DASL, "DASL" },
        { HttpHeader::DAV, "DAV" },
        { HttpHeader::Date, "Date" },
        { HttpHeader::Depth, "Depth" },
        { HttpHeader::Destination, "Destination" },
        { HttpHeader::ETag, "ETag" },
        { HttpHeader::Expect, "Expect" },
        { HttpHeader::Expires, "Expires" },
        { HttpHeader::Forwarded, "Forwarded" },
        { HttpHeader::From, "From" },
        { HttpHeader::Host, "Host" },
        { HttpHeader::HTTP2Settings, "HTTP2-Settings" },
        { HttpHeader::If, "If" },
        { HttpHeader::IfMatch, "If-Match" },
        { HttpHeader::IfModifiedSince, "If-Modified-Since" },
        { HttpHeader::IfNoneMatch, "If-None-Match" },
        { HttpHeader::IfRange, "If-Range" },
        { HttpHeader::IfScheduleTagMatch, "If-Schedule-Tag-Match" },
        { HttpHeader::IfUnmodifiedSince, "If-Unmodified-Since" },
        { HttpHeader::LastModified, "Last-Modified" },
        { HttpHeader::Link, "Link" },
        { HttpHeader::Location, "Location" },
        { HttpHeader::LockToken, "Lock-Token" },
        { HttpHeader::MaxForwards, "Max-Forwards" },
        { HttpHeader::MIMEVersion, "MIME-Version" },
        { HttpHeader::OrderingType, "Ordering-Type" },
        { HttpHeader::Origin, "Origin" },
        { HttpHeader::Overwrite, "Overwrite" },
        { HttpHeader::Position, "Position" },
        { HttpHeader::Pragma, "Pragma" },
        { HttpHeader::Prefer, "Prefer" },
        { HttpHeader::PreferenceApplied, "Preference-Applied" },
        { HttpHeader::ProxyAuthenticate, "Proxy-Authenticate" },
        { HttpHeader::ProxyAuthenticationInfo, "Proxy-Authentication-Info" },
        { HttpHeader::ProxyAuthorization, "Proxy-Authorization" },
        { HttpHeader::PublicKeyPins, "public-key-pins" },
        { HttpHeader::PublicKeyPinsReportOnly, "Public-Key-Pins-Report-Only" },
        { HttpHeader::Range, "Range" },
        { HttpHeader::Referer, "Referer" },
        { HttpHeader::RetryAfter, "Retry-After" },
        { HttpHeader::ScheduleReply, "Schedule-Reply" },
        { HttpHeader::ScheduleTag, "Schedule-Tag" },
        { HttpHeader::SecWebSocketAccept, "Sec-WebSocket-Accept" },
        { HttpHeader::SecWebSocketExtensions, "Sec-WebSocket-Extensions" },
        { HttpHeader::SecWebSocketKey, "Sec-WebSocket-Key" },
        { HttpHeader::SecWebSocketProtocol, "Sec-WebSocket-Protocol" },
        { HttpHeader::SecWebSocketVersion, "Sec-WebSocket-Version" },
        { HttpHeader::Server, "Server" },
        { HttpHeader::SetCookie, "Set-Cookie" },
        { HttpHeader::SLUG, "SLUG" },
        { HttpHeader::StrictTransportSecurity, "Strict-Transport-Security" },
        { HttpHeader::TE, "TE" },
        { HttpHeader::Timeout, "Timeout" },
        { HttpHeader::Topic, "Topic" },
        { HttpHeader::Trailer, "Trailer" },
        { HttpHeader::TransferEncoding, "Transfer-Encoding" },
        { HttpHeader::TTL, "TTL" },
        { HttpHeader::Urgency, "Urgency" },
        { HttpHeader::Upgrade, "Upgrade" },
        { HttpHeader::UserAgent, "User-Agent" },
        { HttpHeader::Vary, "Vary" },
        { HttpHeader::Via, "Via" },
        { HttpHeader::WWWAuthenticate, "WWW-Authenticate" },
        { HttpHeader::Warning, "Warning" },
        { HttpHeader::XContentTypeOptions, "X-Content-Type-Options" },
    };

    HttpResponse::HttpResponse():
        position(0)
    { }


    char* HttpResponse::to_buffer()
    {
        return &buffer[0];
    }

    void HttpResponse::status(int value)
    {
        switch (value) {
            case 100:
                return status(value, "Continue");
            case 101:
                return status(value, "Switching Protocols");
            case 200:
                return status(value, "OK");
            case 201:
                return status(value, "Created");
            case 202:
                return status(value, "Accepted");
            case 203:
                return status(value, "Non-Authoritative Information");
            case 204:
                return status(value, "No Content");
            case 205:
                return status(value, "Reset Content");
            case 206:
                return status(value, "Partial Content");
            case 300:
                return status(value, "Multiple Choices");
            case 301:
                return status(value, "Moved Permanently");
            case 302:
                return status(value, "Found");
            case 303:
                return status(value, "See Other");
            case 304:
                return status(value, "Not Modified");
            case 305:
                return status(value, "Use Proxy");
            case 307:
                return status(value, "Temporary Redirect");
            case 400:
                return status(value, "Bad Request");
            case 401:
                return status(value, "Unauthorized");
            case 402:
                return status(value, "Payment Required");
            case 403:
                return status(value, "Forbidden");
            case 404:
                return status(value, "Not Found");
            case 405:
                return status(value, "Method Not Allowed");
            case 406:
                return status(value, "Not Acceptable");
            case 407:
                return status(value, "Proxy Authentication Required");
            case 408:
                return status(value, "Request Time-out");
            case 409:
                return status(value, "Conflict");
            case 410:
                return status(value, "Gone");
            case 411:
                return status(value, "Length Required");
            case 412:
                return status(value, "Precondition Failed");
            case 413:
                return status(value, "Request Entity Too Large");
            case 414:
                return status(value, "Request-URI Too Large");
            case 415:
                return status(value, "Unsupported Media Type");
            case 416:
                return status(value, "Requested Range Not Satisfiable");
            case 417:
                return status(value, "Expectation Failed");
            case 500:
                return status(value, "Internal Server Error");
            case 501:
                return status(value, "Not Implemented");
            case 502:
                return status(value, "Bad Gateway");
            case 503:
                return status(value, "Service Unavailable");
            case 504:
                return status(value, "Gateway Time-out");
            case 505:
                return status(value, "HTTP Version Not Supported");
            default:
                return status(value, "No Given Reason");
        }
    }

    void HttpResponse::status(int value, const char* reason)
    {
        write_status_line(value, reason);
        int length = strlen(body);
        if (body) {
            header(HttpHeader::ContentLength, std::to_string(length).c_str());
        }
        for (const auto& it : headers) {
            write(token_to_string[it.first]);
            write(": ");
            write(it.second);
            write_newline();
        }
        write_newline();
        if (body) {
            write(body, length);
        }
    }

    void HttpResponse::header(HttpHeader header, const char *value)
    {
        headers.push_back(std::make_pair(header, value));
    }

    size_t HttpResponse::size()
    {
        return buffer.size();
    }

    void HttpResponse::write(const char* text, int length)
    {
        int new_position = position + length;
        for (int i = 0; i < length; i++) {
            buffer.push_back(text[i]);
        }
        position = new_position;
    }

    void HttpResponse::write(const char* text)
    {
        int length = strlen(text);
        write(text, length);
    }

    void HttpResponse::write_newline()
    {
        write("\r\n");
    }


    void HttpResponse::write_space()
    {
        write(" ");
    }

    void HttpResponse::write_status_line(int status, const char* reason)
    {
        write("HTTP/1.1 ");
        write(std::to_string(status).c_str());
        write_space();
        write(reason);
        write_newline();
    }
}
