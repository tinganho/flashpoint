#include "memory_pool.h"
#include <memory>

#define start_address_of_block(x) x * block_size

namespace flashpoint::lib {

MemoryPool::MemoryPool(std::size_t total_size, std::size_t block_size)
{
    this->total_size = total_size;
    this->block_size = block_size;
    this->start_address_of_pool = (char*)malloc(total_size);
    this->reset();
}

std::size_t MemoryPool::allocate_block()
{
    std::size_t block = free_blocks.top();
    free_blocks.pop();
    return block;
}

MemoryPoolTicket* MemoryPool::take_ticket()
{
    auto block = allocate_block();
    auto ticket = (MemoryPoolTicket*)(start_address_of_pool + start_address_of_block(block));
    ticket->block = block;
    ticket->offset = sizeof(MemoryPoolTicket);
    return ticket;
}

void MemoryPool::return_ticket(MemoryPoolTicket* ticket)
{
    free_blocks.push(ticket->block);
}

void* MemoryPool::allocate(std::size_t size, std::size_t alignment, MemoryPoolTicket* ticket)
{
    std::size_t padding = 0;
    auto current_address = (std::size_t)(start_address_of_pool + start_address_of_block(ticket->block) + ticket->offset);

    if (alignment != 0 && ticket->offset % alignment != 0) {
        const std::size_t multiplier = (current_address / alignment) + 1;
        const std::size_t aligned_address = multiplier * alignment;
        padding = aligned_address - current_address;
    }

    if (ticket->offset + padding + size > block_size) {
        ticket->block = allocate_block();
        ticket->offset = 0;
        if (ticket->block == -1) {
            throw std::logic_error("Out of memory: trying to allocate a block of memory for a request.");
        }
        return allocate(size, alignment, ticket);
    }

    ticket->offset += padding + size;

#ifdef _DEBUG
    std::cout << "A" << "\t@C " << (void*) current_address << "\t@R " << (void*) next_address << "\tO " << request->offset << "\tP " << padding << std::endl;
#endif

    return (void*)current_address;
}

void MemoryPool::reset()
{
    std::size_t blocks = total_size / block_size;
    for (std::size_t i = 0; i < blocks; i++) {
        free_blocks.push(i);
    }
}

}