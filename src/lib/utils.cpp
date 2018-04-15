//
// Created by Tingan Ho on 2018-04-09.
//

#include "utils.h"

namespace flash::lib {
    template<typename K, typename V>
    std::map<V, K> reverse_map(const std::map <K, V> &input) {
        typename std::map<V, K> output;
        for (auto it = input.begin(); it != input.end(); it++) {
            output.emplace(it->second, it->first);
        }
        return output;
    }
}