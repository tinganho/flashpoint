#ifndef FLASH_HTTP_SCANNER_H
#define FLASH_HTTP_SCANNER_H

#include <map>
#include <unordered_map>
#include <stack>
#include <vector>
#include <cstring>

namespace flashpoint::program {

    enum HttpMethod {
        None,
        Get,
        Post,
        Put,
        Delete,
        Patch,
        Head,
        Connect,
        Options,
        Trace,
    };

    enum class RequestLineToken {
        None,
        Crlf,
        Question,

        Protocol,
        AbsolutePath,
        Query,
        HttpVersion1_1,

        EndOfRequestTarget,
    };

    /**
     * https://www.iana.org/assignments/message-headers/message-headers.xhtml
     */
    enum class HttpHeader {
        Unknown,
        Accept,
        AcceptCharset,
        AcceptEncoding,
        AcceptLanguage,
        AcceptPost,
        AcceptRanges,
        Age,
        Allow,
        ALPN,
        AltSvc,
        AltUsed,
        AuthenticationInfo,
        Authorization,
        CacheControl,
        CalDAVTimezones,
        Connection,
        ContentDisposition,
        ContentEncoding,
        ContentLanguage,
        ContentLength,
        ContentLocation,
        ContentRange,
        ContentType,
        Cookie,
        DASL,
        DAV,
        Date,
        Depth,
        Destination,
        ETag,
        Expect,
        Expires,
        Forwarded,
        From,
        Host,
        HTTP2Settings,
        If,
        IfMatch,
        IfModifiedSince,
        IfNoneMatch,
        IfRange,
        IfScheduleTagMatch,
        IfUnmodifiedSince,
        LastModified,
        Link,
        Location,
        LockToken,
        MaxForwards,
        MIMEVersion,
        OrderingType,
        Origin,
        Overwrite,
        Position,
        Pragma,
        Prefer,
        PreferenceApplied,
        ProxyAuthenticate,
        ProxyAuthenticationInfo,
        ProxyAuthorization,
        PublicKeyPins,
        PublicKeyPinsReportOnly,
        Range,
        Referer,
        RetryAfter,
        ScheduleReply,
        ScheduleTag,
        SecWebSocketAccept,
        SecWebSocketExtensions,
        SecWebSocketKey,
        SecWebSocketProtocol,
        SecWebSocketVersion,
        Server,
        SetCookie,
        SLUG,
        StrictTransportSecurity,
        TE,
        Timeout,
        Topic,
        Trailer,
        TransferEncoding,
        TTL,
        Urgency,
        Upgrade,
        UserAgent,
        Vary,
        Via,
        WWWAuthenticate,
        Warning,
        XContentTypeOptions,

        End,
    };

    struct char_compare {
        bool operator()(const char* a, const char* b) const {
            return strcmp(a, b) < 0;
        }
    };

    const std::map<const char*, HttpHeader, char_compare> string_to_token = {
        { "accept", HttpHeader::Accept },
        { "accept-charset", HttpHeader::AcceptCharset },
        { "accept-encoding", HttpHeader::AcceptEncoding },
        { "accept-language", HttpHeader::AcceptLanguage },
        { "accept-post", HttpHeader::AcceptPost },
        { "accept-ranges", HttpHeader::AcceptRanges },
        { "age", HttpHeader::Age },
        { "allow", HttpHeader::Allow },
        { "alpn", HttpHeader::ALPN },
        { "alt-svc", HttpHeader::AltSvc },
        { "alt-used", HttpHeader::AltUsed },
        { "authentication-info", HttpHeader::AuthenticationInfo },
        { "authorization", HttpHeader::Authorization },
        { "cache-control", HttpHeader::CacheControl },
        { "caldav-timezones", HttpHeader::CalDAVTimezones },
        { "connection", HttpHeader::Connection },
        { "content-disposition", HttpHeader::ContentDisposition },
        { "content-encoding", HttpHeader::ContentEncoding },
        { "content-language", HttpHeader::ContentLanguage },
        { "content-length", HttpHeader::ContentLength },
        { "content-location", HttpHeader::ContentLocation },
        { "content-range", HttpHeader::ContentRange },
        { "content-type", HttpHeader::ContentType },
        { "cookie", HttpHeader::Cookie },
        { "dasl", HttpHeader::DASL },
        { "dav", HttpHeader::DAV },
        { "date", HttpHeader::Date },
        { "depth", HttpHeader::Depth },
        { "destination", HttpHeader::Destination },
        { "etag", HttpHeader::ETag },
        { "expect", HttpHeader::Expect },
        { "expires", HttpHeader::Expires },
        { "forwarded", HttpHeader::Forwarded },
        { "from", HttpHeader::From },
        { "host", HttpHeader::Host },
        { "http2-settings", HttpHeader::HTTP2Settings },
        { "if", HttpHeader::If },
        { "if-match", HttpHeader::IfMatch },
        { "if-modified-since", HttpHeader::IfModifiedSince },
        { "if-none-match", HttpHeader::IfNoneMatch },
        { "if-range", HttpHeader::IfRange },
        { "if-schedule-tag-match", HttpHeader::IfScheduleTagMatch },
        { "if-unmodified-since", HttpHeader::IfUnmodifiedSince },
        { "last-modified", HttpHeader::LastModified },
        { "link", HttpHeader::Link },
        { "location", HttpHeader::Location },
        { "lock-token", HttpHeader::LockToken },
        { "max-forwards", HttpHeader::MaxForwards },
        { "mime-version", HttpHeader::MIMEVersion },
        { "ordering-type", HttpHeader::OrderingType },
        { "origin", HttpHeader::Origin },
        { "overwrite", HttpHeader::Overwrite },
        { "position", HttpHeader::Position },
        { "pragma", HttpHeader::Pragma },
        { "prefer", HttpHeader::Prefer },
        { "preference-applied", HttpHeader::PreferenceApplied },
        { "proxy-authenticate", HttpHeader::ProxyAuthenticate },
        { "proxy-authentication-info", HttpHeader::ProxyAuthenticationInfo },
        { "proxy-authorization", HttpHeader::ProxyAuthorization },
        { "public-key-pins", HttpHeader::PublicKeyPins },
        { "public-key-pins-report-only", HttpHeader::PublicKeyPinsReportOnly },
        { "range", HttpHeader::Range },
        { "referer", HttpHeader::Referer },
        { "retry-after", HttpHeader::RetryAfter },
        { "schedule-reply", HttpHeader::ScheduleReply },
        { "schedule-tag", HttpHeader::ScheduleTag },
        { "sec-webSocket-accept", HttpHeader::SecWebSocketAccept },
        { "sec-webSocket-extensions", HttpHeader::SecWebSocketExtensions },
        { "sec-webSocket-key", HttpHeader::SecWebSocketKey },
        { "sec-webSocket-protocol", HttpHeader::SecWebSocketProtocol },
        { "sec-webSocket-version", HttpHeader::SecWebSocketVersion },
        { "server", HttpHeader::Server },
        { "set-cookie", HttpHeader::SetCookie },
        { "slug", HttpHeader::SLUG },
        { "strict-transport-security", HttpHeader::StrictTransportSecurity },
        { "te", HttpHeader::TE },
        { "timeout", HttpHeader::Timeout },
        { "topic", HttpHeader::Topic },
        { "trailer", HttpHeader::Trailer },
        { "transfer-encoding", HttpHeader::TransferEncoding },
        { "ttl", HttpHeader::TTL },
        { "urgency", HttpHeader::Urgency },
        { "upgrade", HttpHeader::Upgrade },
        { "user-agent", HttpHeader::UserAgent },
        { "vary", HttpHeader::Vary },
        { "via", HttpHeader::Via },
        { "www-authenticate", HttpHeader::WWWAuthenticate },
        { "warning", HttpHeader::Warning },
        { "x-content-type-options", HttpHeader::XContentTypeOptions },
    };

    struct SavedTextCursor {
        long long position;
        long long start_position;
        long long end_position;
    };

    enum ParserMode {
        Method,
        AbsolutePath,
        Query,
        HttpVersion,
        HeaderField,
        HeaderValue,
    };

    class HttpScanner final {
    public:
        HttpScanner(const char* text, std::size_t length);
        void scan_request_target();
        HttpHeader scan_header();
        char* scan_absolute_path();
        char* scan_query();
        char* scan_body(unsigned int length);
        RequestLineToken scan_http_version();
        HttpMethod scan_method();
        char* get_lower_cased_value() const;
        char* get_token_value() const;
        char* get_header_value();
        bool scan_optional(char ch);
        void scan_expected(char ch);

        bool next_char_is(char ch);
        void scan_rest_of_line();
    private:
        long long position;
        long long start_position;
        long long end_position;
        char* current_header;
        ParserMode parser_mode;
        std::stack<SavedTextCursor> saved_text_cursors;
        const char* text;
        long long size;
        bool is_pchar(char ch);
        void save();
        void revert();
        char peek_next_char();
        bool scan_field_content();
        void scan_header_value();
        bool is_vchar(char ch);
        bool is_obs_text(char ch);
        bool is_unreserverd_char(char ch);
        bool is_sub_delimiter(char ch);
        bool is_header_field_start(char ch);
        bool is_header_field_part(char ch);
        bool is_method_part(char ch);
        void increment_position();
        void set_token_start_position();
        char current_char();
        HttpHeader get_header(char *ch);
        const std::map<HttpHeader, const char*> header_enum_to_string;
        const std::map<const char*, HttpHeader, char_compare> header_to_token_enum;
    };
}



#endif //FLASH_HTTP_SCANNER_H
