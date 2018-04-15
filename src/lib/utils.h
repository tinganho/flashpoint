//
// Created by Tingan Ho on 2018-04-09.
//

#ifndef FLASH_UTILS_H
#define FLASH_UTILS_H

#include <map>

namespace flash::lib {
    template<typename K, typename V>
    std::map<V, K> reverse_map(const std::map <K, V> &input);
}

#endif //FLASH_UTILS_H
