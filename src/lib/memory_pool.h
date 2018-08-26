#ifndef FLASHPOINT_REQUEST_ALLOACTOR_H
#define FLASHPOINT_REQUEST_ALLOACTOR_H

#include <stack>
#include <vector>

namespace flashpoint::lib {

class MemoryPoolTicket {
public:

    std::size_t
    offset;

    std::vector<std::size_t>
    block_indices;

    std::size_t
    current_block_index;

    MemoryPoolTicket(std::size_t offset, std::size_t block):
        offset(offset),
        block_indices(block)
    { }
};

class MemoryPool {
public:

    MemoryPool(std::size_t total_size, std::size_t block_size);

    void*
    Allocate(std::size_t size, std::size_t alignment, MemoryPoolTicket *ticket);

    MemoryPoolTicket*
    TakeTicket();

    void
    ReturnTicket(MemoryPoolTicket *ticket);

    std::size_t
    AllocateBlock();

    void
    Reset();

private:

    std::size_t
    block_size;

    std::size_t
    total_size;

    char*
    start_address_of_pool;

    std::stack<std::size_t>
    free_blocks;
};

}


#endif //FLASHPOINT_REQUEST_ALLOACTOR_H
