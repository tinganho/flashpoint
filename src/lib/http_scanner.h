//
// Created by Tingan Ho on 2018-04-08.
//

#ifndef FLASH_HTTP_SCANNER_H
#define FLASH_HTTP_SCANNER_H

#include <map>
#include <stack>
#include <vector>

namespace flash::lib {

    enum Character {
        NullCharacter = 0x00,

        Backspace = 0x08,             // \b
        HorizontalTab = 0x09,
        LineFeed = 0x0A,              // \n
        VerticalTab = 0x0B,
        FormFeed = 0x0C,
        CarriageReturn = 0x0D,        // \r
        Tab = 0x09,                   // \t
        Space = 0x20,

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
	    Backtick = 0x60,              // `
        Bar = 0x7C,                   // |
        Caret = 0x5E,                 // ^
        CloseBrace = 0x7D,            // }
        CloseBracket = 0x5D,          // ]
        CloseParen = 0x29,            // )
        Colon = 0x3A,                 // :
        Comma = 0x2C,                 // ,
        Dot = 0x2E,                   // .
        DoubleQuote = 0x22,           // "
        Equal = 0x3D,                 // =
        Exclamation = 0x21,           // !
        Dollar = 0x24,                // $
        GreaterThan = 0x3E,           // >
        Hash = 0x23,                  // #
        LessThan = 0x3C,              // <
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
        Tilde = 0x7E,                 // ~
        Underscore = 0x5F,            // _

        MaxAsciiCharacter = 0x7F,
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
        { "Accept", HttpHeader::Accept },
        { "Accept-Charset", HttpHeader::AcceptCharset },
        { "Accept-Encoding", HttpHeader::AcceptEncoding },
        { "Accept-Language", HttpHeader::AcceptLanguage },
        { "Accept-Post", HttpHeader::AcceptPost },
        { "Accept-Ranges", HttpHeader::AcceptRanges },
        { "Age", HttpHeader::Age },
        { "Allow", HttpHeader::Allow },
        { "ALPN", HttpHeader::ALPN },
        { "Alt-Svc", HttpHeader::AltSvc },
        { "Alt-Used", HttpHeader::AltUsed },
        { "Authentication-Info", HttpHeader::AuthenticationInfo },
        { "Authorization", HttpHeader::Authorization },
        { "Cache-Control", HttpHeader::CacheControl },
        { "CalDAV-Timezones", HttpHeader::CalDAVTimezones },
        { "Connection", HttpHeader::Connection },
        { "Content-Disposition", HttpHeader::ContentDisposition },
        { "Content-Encoding", HttpHeader::ContentEncoding },
        { "Content-Language", HttpHeader::ContentLanguage },
        { "Content-Length", HttpHeader::ContentLength },
        { "Content-Location", HttpHeader::ContentLocation },
        { "Content-Range", HttpHeader::ContentRange },
        { "Content-Type", HttpHeader::ContentType },
        { "Cookie", HttpHeader::Cookie },
        { "DASL", HttpHeader::DASL },
        { "DAV", HttpHeader::DAV },
        { "Date", HttpHeader::Date },
        { "Depth", HttpHeader::Depth },
        { "Destination", HttpHeader::Destination },
        { "ETag", HttpHeader::ETag },
        { "Expect", HttpHeader::Expect },
        { "Expires", HttpHeader::Expires },
        { "Forwarded", HttpHeader::Forwarded },
        { "From", HttpHeader::From },
        { "Host", HttpHeader::Host },
        { "HTTP2-Settings", HttpHeader::HTTP2Settings },
        { "If", HttpHeader::If },
        { "If-Match", HttpHeader::IfMatch },
        { "If-Modified-Since", HttpHeader::IfModifiedSince },
        { "If-None-Match", HttpHeader::IfNoneMatch },
        { "If-Range", HttpHeader::IfRange },
        { "If-Schedule-Tag-Match", HttpHeader::IfScheduleTagMatch },
        { "If-Unmodified-Since", HttpHeader::IfUnmodifiedSince },
        { "Last-Modified", HttpHeader::LastModified },
        { "Link", HttpHeader::Link },
        { "Location", HttpHeader::Location },
        { "Lock-Token", HttpHeader::LockToken },
        { "Max-Forwards", HttpHeader::MaxForwards },
        { "MIME-Version", HttpHeader::MIMEVersion },
        { "Ordering-Type", HttpHeader::OrderingType },
        { "Origin", HttpHeader::Origin },
        { "Overwrite", HttpHeader::Overwrite },
        { "Position", HttpHeader::Position },
        { "Pragma", HttpHeader::Pragma },
        { "Prefer", HttpHeader::Prefer },
        { "Preference-Applied", HttpHeader::PreferenceApplied },
        { "Proxy-Authenticate", HttpHeader::ProxyAuthenticate },
        { "Proxy-Authentication-Info", HttpHeader::ProxyAuthenticationInfo },
        { "Proxy-Authorization", HttpHeader::ProxyAuthorization },
        { "Public-Key-Pins", HttpHeader::PublicKeyPins },
        { "Public-Key-Pins-Report-Only", HttpHeader::PublicKeyPinsReportOnly },
        { "Range", HttpHeader::Range },
        { "Referer", HttpHeader::Referer },
        { "Retry-After", HttpHeader::RetryAfter },
        { "Schedule-Reply", HttpHeader::ScheduleReply },
        { "Schedule-Tag", HttpHeader::ScheduleTag },
        { "Sec-WebSocket-Accept", HttpHeader::SecWebSocketAccept },
        { "Sec-WebSocket-Extensions", HttpHeader::SecWebSocketExtensions },
        { "Sec-WebSocket-Key", HttpHeader::SecWebSocketKey },
        { "Sec-WebSocket-Protocol", HttpHeader::SecWebSocketProtocol },
        { "Sec-WebSocket-Version", HttpHeader::SecWebSocketVersion },
        { "Server", HttpHeader::Server },
        { "Set-Cookie", HttpHeader::SetCookie },
        { "SLUG", HttpHeader::SLUG },
        { "Strict-Transport-Security", HttpHeader::StrictTransportSecurity },
        { "TE", HttpHeader::TE },
        { "Timeout", HttpHeader::Timeout },
        { "Topic", HttpHeader::Topic },
        { "Trailer", HttpHeader::Trailer },
        { "Transfer-Encoding", HttpHeader::TransferEncoding },
        { "TTL", HttpHeader::TTL },
        { "Urgency", HttpHeader::Urgency },
        { "Upgrade", HttpHeader::Upgrade },
        { "User-Agent", HttpHeader::UserAgent },
        { "Vary", HttpHeader::Vary },
        { "Via", HttpHeader::Via },
        { "WWW-Authenticate", HttpHeader::WWWAuthenticate },
        { "Warning", HttpHeader::Warning },
        { "X-Content-Type-Options", HttpHeader::XContentTypeOptions },
    };

    struct SavedTextCursor {
        unsigned int position;
        unsigned int start_position;
        unsigned int end_position;
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
        RequestLineToken scan_http_version();
        HttpMethod scan_method();
        char* get_token_value() const;
        char* get_header_value();
        bool scan_optional(char ch);
        void scan_expected(char ch);

        bool next_char_is(char ch);
        void scan_rest_of_line();
    private:
        unsigned int position;
        unsigned int start_position;
        unsigned int end_position;
        char* current_header;
        ParserMode parser_mode;
        std::stack<SavedTextCursor> saved_text_cursors;
        char* text;
        unsigned int size;
        bool is_pchar(char ch);
        void save();
        void revert();
        char peek_next_char();
        bool is_vchar(char ch);
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
