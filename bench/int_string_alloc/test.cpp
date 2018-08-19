#include <cstddef>
#include <cstdio>
#include <chrono>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <cmath>

int main() {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();;
    int y = 1000000;
    int tmp_y;

    for (int i = 0; i < 100000; i++) {
        int number_of_chars = 0;
        tmp_y = y;
        while (tmp_y /= 10) {
            number_of_chars++;
        }
        char* content_length = (char*)malloc(number_of_chars);
        sprintf(content_length, "%d", y);
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::size_t duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Duration: " << duration << "us" << std::endl;
}