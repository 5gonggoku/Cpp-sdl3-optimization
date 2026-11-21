#include "memory/AllocCounter.h"

#include <cstdlib>
#include <new>

std::uint64_t g_allocCount = 0;

void* operator new(std::size_t size)
{
    ++g_allocCount;
    void* p = std::malloc(size ? size : 1);
    if (!p) std::abort();
    return p;
}

void* operator new[](std::size_t size)
{
    ++g_allocCount;
    void* p = std::malloc(size ? size : 1);
    if (!p) std::abort();
    return p;
}

void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
