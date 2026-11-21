#include <cstddef>
#include <cstdio>
#include <set>
#include <vector>

#include "memory/Pool.h"

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_failures;                                                             \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);               \
        }                                                                             \
    } while (0)

struct Probe {
    static int constructed;
    static int destroyed;
    int value;
    explicit Probe(int v) : value(v) { ++constructed; }
    ~Probe() { ++destroyed; }
    Probe(const Probe&) = delete;
    Probe& operator=(const Probe&) = delete;
};
int Probe::constructed = 0;
int Probe::destroyed = 0;

void ResetProbe() { Probe::constructed = 0; Probe::destroyed = 0; }

void TestAcquireRelease()
{
    ResetProbe();
    Pool<Probe, 4> pool;
    CHECK(pool.LiveCount() == 0);
    CHECK(pool.Capacity() == 0);

    Probe* a = pool.Acquire(10);
    Probe* b = pool.Acquire(20);
    CHECK(a != nullptr && b != nullptr && a != b);
    CHECK(a->value == 10 && b->value == 20);
    CHECK(pool.LiveCount() == 2);
    CHECK(pool.Capacity() == 4 && pool.BlockCount() == 1);
    CHECK(Probe::constructed == 2 && Probe::destroyed == 0);

    CHECK(pool.Release(a));
    CHECK(pool.LiveCount() == 1);
    CHECK(Probe::destroyed == 1);
    CHECK(b->value == 20);
}

void TestSlotReuse()
{
    Pool<Probe, 4> pool;
    Probe* a = pool.Acquire(1);
    pool.Release(a);
    Probe* b = pool.Acquire(2);
    CHECK(b == a);
    CHECK(b->value == 2);
    CHECK(pool.BlockCount() == 1);
}

void TestBlockGrowthKeepsAddresses()
{
    Pool<Probe, 4> pool;
    std::vector<Probe*> objs;
    std::vector<Probe*> addrs;
    for (int i = 0; i < 4; ++i) { objs.push_back(pool.Acquire(i)); addrs.push_back(objs.back()); }
    CHECK(pool.BlockCount() == 1);

    for (int i = 4; i < 10; ++i) objs.push_back(pool.Acquire(i));
    CHECK(pool.BlockCount() == 3);
    CHECK(pool.Capacity() == 12);
    CHECK(pool.LiveCount() == 10);

    bool addressesStable = true;
    for (int i = 0; i < 4; ++i) {
        if (objs[static_cast<std::size_t>(i)] != addrs[static_cast<std::size_t>(i)]) addressesStable = false;
    }
    CHECK(addressesStable);

    bool valuesOk = true;
    std::set<Probe*> unique;
    for (int i = 0; i < 10; ++i) {
        if (objs[static_cast<std::size_t>(i)]->value != i) valuesOk = false;
        unique.insert(objs[static_cast<std::size_t>(i)]);
    }
    CHECK(valuesOk);
    CHECK(unique.size() == 10);
}

void TestInvalidRelease()
{
    ResetProbe();
    Pool<Probe, 4> pool;
    Pool<Probe, 4> other;
    Probe* a = pool.Acquire(1);

    CHECK(pool.Release(a));
    CHECK(!pool.Release(a));
    CHECK(Probe::destroyed == 1);
    CHECK(pool.LiveCount() == 0);

    Probe* b = pool.Acquire(2);
    CHECK(!other.Release(b));
    CHECK(pool.LiveCount() == 1);

    Probe stackObject(3);
    CHECK(!pool.Release(&stackObject));
    CHECK(pool.LiveCount() == 1);
}

void TestDestructorDestroysLive()
{
    ResetProbe();
    {
        Pool<Probe, 4> pool;
        Probe* a = pool.Acquire(1);
        pool.Acquire(2);
        pool.Acquire(3);
        pool.Release(a);
        CHECK(Probe::constructed == 3 && Probe::destroyed == 1);
    }
    CHECK(Probe::destroyed == 3);
}

void TestChurnKeepsCapacity()
{

    Pool<Probe, 8> pool;
    std::vector<Probe*> live;
    for (int i = 0; i < 6; ++i) live.push_back(pool.Acquire(i));
    const std::size_t cap = pool.Capacity();
    for (int round = 0; round < 1000; ++round) {
        pool.Release(live.front());
        live.erase(live.begin());
        live.push_back(pool.Acquire(round));
    }
    CHECK(pool.Capacity() == cap);
    CHECK(pool.BlockCount() == 1);
    CHECK(pool.LiveCount() == 6);
}

}

int main()
{
    TestAcquireRelease();
    TestSlotReuse();
    TestBlockGrowthKeepsAddresses();
    TestInvalidRelease();
    TestDestructorDestroysLive();
    TestChurnKeepsCapacity();

    std::printf("pool_tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
