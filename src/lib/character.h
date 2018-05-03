#ifndef FLASHPOINT_CHARACTER_H
#define FLASHPOINT_CHARACTER_H

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
}
#endif //FLASHPOINT_CHARACTER_H
