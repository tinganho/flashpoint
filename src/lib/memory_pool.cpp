#include "memory_pool.h"
#include <memory>

#define start_address_of_block(x) x * block_size

namespace flashpoint::lib {

MemoryPool::MemoryPool(std::size_t total_size, std::size_t block_size)
{
    this->total_size = total_size;
    this->block_size = block_size;
    this->start_address_of_pool = (char*)malloc(total_size);
    this->Reset();
}

std::size_t
MemoryPool::AllocateBlock()
{
    std::size_t block = free_blocks.top();
    free_blocks.pop();
    return block;
}

MemoryPoolTicket*
MemoryPool::TakeTicket()
{
    auto block_index = AllocateBlock();
    auto ticket = (MemoryPoolTicket*)(start_address_of_pool + start_address_of_block(block_index));
    ticket->block_indices.push_back(block_index);
    ticket->current_block_index = block_index;
    ticket->offset = sizeof(MemoryPoolTicket);
    return ticket;
}

void
MemoryPool::ReturnTicket(MemoryPoolTicket *ticket)
{
    for (const auto& block_index : ticket->block_indices) {
        free_blocks.push(block_index);
    }
}

void*
MemoryPool::Allocate(std::size_t size, std::size_t alignment, MemoryPoolTicket *ticket)
{
    std::size_t padding = 0;
    auto current_address = (std::size_t)(start_address_of_pool + start_address_of_block(ticket->current_block_index) + ticket->offset);

    if (alignment != 0 && ticket->offset % alignment != 0) {
        const std::size_t multiplier = (current_address / alignment) + 1;
        const std::size_t aligned_address = multiplier * alignment;
        padding = aligned_address - current_address;
    }

    if (ticket->offset + padding + size > block_size) {
        auto block_index = AllocateBlock();
        ticket->block_indices.push_back(block_index);
        ticket->current_block_index = block_index;
        ticket->offset = 0;
        if (block_index == -1) {
            throw std::logic_error("Out of memory: trying to allocate a block of memory for a request.");
        }
        return Allocate(size, alignment, ticket);
    }

    ticket->offset += padding + size;

#ifdef _DEBUG
    std::cout << "A" << "\t@C " << (void*) current_address << "\t@R " << (void*) next_address << "\tO " << request->offset << "\tP " << padding << std::endl;
#endif

    return (void*)current_address;
}

void
MemoryPool::Reset() {
    std::size_t blocks = total_size / block_size;
    for (std::size_t i = 0; i < blocks; i++) {
        free_blocks.push(i);
    }
}

}