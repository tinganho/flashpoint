#ifndef FLASH_HTTP_SCANNER_H
#define FLASH_HTTP_SCANNER_H

#include <map>
#include <stack>
#include <vector>

namespace flashpoint::program {

    enum Character {
        NullCharacter = 0x00,

        _ = 0x5F,
        $ = 0x24,

        _0 = 0x30,
        _1 = 0x31,
        _2 = 0x32,
        _3 = 0x33,
        _4 = 0x34,
        _5 = 0x35,
        _6 = 0x36,
        _7 = 0x37,
        _8 = 0x38,
        _9 = 0x39,

        a = 0x61,
        b = 0x62,
        c = 0x63,
        d = 0x64,
        e = 0x65,
        f = 0x66,
        g = 0x67,
        h = 0x68,
        i = 0x69,
        j = 0x6A,
        k = 0x6B,
        l = 0x6C,
        m = 0x6D,
        n = 0x6E,
        o = 0x6F,
        p = 0x70,
        q = 0x71,
        r = 0x72,
        s = 0x73,
        t = 0x74,
        u = 0x75,
        v = 0x76,
        w = 0x77,
        x = 0x78,
        y = 0x79,
        z = 0x7A,

        A = 0x41,
        B = 0x42,
        C = 0x43,
        D = 0x44,
        E = 0x45,
        F = 0x46,
        G = 0x47,
        H = 0x48,
        I = 0x49,
        J = 0x4A,
        K = 0x4B,
        L = 0x4C,
        M = 0x4D,
        N = 0x4E,
        O = 0x4F,
        P = 0x50,
        Q = 0x51,
        R = 0x52,
        S = 0x53,
        T = 0x54,
        U = 0x55,
        V = 0x56,
        W = 0x57,
        X = 0x58,
        Y = 0x59,
        Z = 0x5a,

        Ampersand = 0x26,             // &
        Asterisk = 0x2A,              // *
        At = 0x40,                    // @
        Backslash = 0x5C,             // \
        Backspace = 0x08,             // \b
	    Backtick = 0x60,              // `
        Bar = 0x7C,                   // |
        Caret = 0x5E,                 // ^
        CarriageReturn = 0x0D,        // \r
        CloseBrace = 0x7D,            // }
        CloseBracket = 0x5D,          // ]
        CloseParen = 0x29,            // )
        Colon = 0x3A,                 // :
        Comma = 0x2C,                 // ,
        Dot = 0x2E,                   // .
        DoubleQuote = 0x22,           // "
        Equal = 0x3D,                 // =
        Exclamation = 0x21,           // !
        FormFeed = 0x0C,              // [FORM_FEED]
        Dollar = 0x24,                // $
        GreaterThan = 0x3E,           // >
        Hash = 0x23,                  // #
        HorizontalTab = 0x09,         // [HORIZONTAL_TAB]
        LessThan = 0x3C,              // <
        LineFeed = 0x0A,              // \n
        Minus = 0x2D,                 // -
        Dash = Minus,                 // -
        OpenBrace = 0x7B,             // {
        OpenBracket = 0x5B,           // [
        OpenParen = 0x28,             // (
        Percent = 0x25,               // %
        Plus = 0x2B,                  // +
        Question = 0x3F,              // ?
        Semicolon = 0x3B,             // ;
        SingleQuote = 0x27,           // '
        Slash = 0x2F,                 // /
        Space = 0x20,                 // [Space]
        Tab = 0x09,                   // \t
        Tilde = 0x7E,                 // ~
        Underscore = 0x5F,            // _
        VerticalTab = 0x0B,           // [VERTICAL_TAB]

        MaxAsciiCharacter = 0x7F,     // DEL
    };

    enum class HttpMethod {
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

    enum class Header {
    };

    struct char_compare {
        bool operator()(const char* a, const char* b) const {
            return std::strcmp(a, b) < 0;
        }
    };

    enum class Result {
        False,
        True,
        Unknown,
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
        HttpScanner(char* text, unsigned int length);
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
        char* text;
        unsigned int size;
        bool is_pchar(char ch);
        void save();
        void revert();
        char peek_next_char();
        bool scan_field_content();
        void scan_header_value();
        bool is_vchar(char ch);
        bool is_obs_text(char ch);
        bool is_tchar(char ch);
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
