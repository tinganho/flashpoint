//
// Created by Tingan Ho on 2018-09-02.
//

#ifndef FLASHPOINT_TEXTCURSOR_H
#define FLASHPOINT_TEXTCURSOR_H

namespace flashpoint::test {

struct ScanningTextCursor {
    unsigned long long position;
    unsigned long long line;
    unsigned long long column;
    unsigned long long token_start_position;
    unsigned long long token_start_line;
    unsigned long long token_start_column;

    ScanningTextCursor(
        unsigned long long position,
        unsigned long long line,
        unsigned long long column,
        unsigned long long start_position,
        unsigned long long start_line,
        unsigned long long start_column):

        position(position),
        line(line),
        column(column),
        token_start_position(start_position),
        token_start_line(start_line),
        token_start_column(start_column)
    { }
};

}
#endif //FLASHPOINT_TEXTCURSOR_H
