#ifndef FLASHPOINT_REQUEST_ALLOACTOR_H
#define FLASHPOINT_REQUEST_ALLOACTOR_H

#include <stack>
#include <vector>

namespace flashpoint::lib {

    class MemoryPool;

    class MemoryPoolTicket {
    public:
        std::size_t offset;
        std::size_t block;

        MemoryPoolTicket(std::size_t offset, std::size_t block):
            offset(offset),
            block(block)
        { }
    };

    class MemoryPool {
    public:
        MemoryPool(const std::size_t total_size, const std::size_t block_size);
        void* allocate(const std::size_t size, const std::size_t alignment, MemoryPoolTicket *ticket);
        MemoryPoolTicket* take_ticket();
        void return_ticket(MemoryPoolTicket* ticket);
        std::size_t allocate_block();
        void reset();
    private:
        std::size_t block_size;
        std::size_t total_size;
        char* start_address_of_pool;
        std::stack<std::size_t> free_blocks;
    };
}


#endif //FLASHPOINT_REQUEST_ALLOACTOR_H
