#include <cstdio>
#include <cstring>

#include "memory/Pool.h"

namespace {
struct Item {
    int value;
    explicit Item(int v) : value(v) {}
};
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: pool_asan_probe released|unused\n");
        return 2;
    }

    Pool<Item, 4> pool;
    Item* a = pool.Acquire(7);
    volatile int sink = 0;

    if (std::strcmp(argv[1], "released") == 0) {
        pool.Release(a);
        sink = a->value;
    } else if (std::strcmp(argv[1], "unused") == 0) {
        Item* neighbor = a + 1;
        sink = neighbor->value;
    } else {
        std::fprintf(stderr, "unknown scenario: %s\n", argv[1]);
        return 2;
    }

    std::printf("ASan이 잡지 못함 (sink=%d)\n", static_cast<int>(sink));
    return 0;
}
