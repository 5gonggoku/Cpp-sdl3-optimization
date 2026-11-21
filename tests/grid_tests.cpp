#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "physics/SpatialGrid.h"

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

constexpr float kW = 1280.0f;
constexpr float kH = 720.0f;

struct Item {
    Vector2 pos;
    float radius;
    bool alive;
};

int BruteFirst(const std::vector<Item>& items, Vector2 q, float queryRadius, int* checks = nullptr)
{
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (!items[i].alive) continue;
        if (checks) ++*checks;
        const Vector2 d = q - items[i].pos;
        const float rs = queryRadius + items[i].radius;
        if (d.LengthSq() <= rs * rs) return static_cast<int>(i);
    }
    return -1;
}

int GridFirst(const SpatialGrid& grid, const std::vector<Item>& items, Vector2 q, float queryRadius, int* checks = nullptr)
{
    return grid.FindFirst(q, [&](int index) {
        const Item& it = items[static_cast<std::size_t>(index)];
        if (!it.alive) return false;
        if (checks) ++*checks;
        const Vector2 d = q - it.pos;
        const float rs = queryRadius + it.radius;
        return d.LengthSq() <= rs * rs;
    });
}

void BuildGrid(SpatialGrid& grid, const std::vector<Item>& items)
{
    grid.Build(items.size(), [&](std::size_t i) { return items[i].pos; });
}

void TestEmptyAndSingle()
{
    SpatialGrid grid;
    grid.Init(kW, kH, 39.0f);
    std::vector<Item> items;
    BuildGrid(grid, items);
    CHECK(GridFirst(grid, items, Vector2(100, 100), 6.0f) == -1);

    items.push_back({ Vector2(100, 100), 16.0f, true });
    BuildGrid(grid, items);
    CHECK(GridFirst(grid, items, Vector2(100, 100), 6.0f) == 0);
    CHECK(GridFirst(grid, items, Vector2(121, 100), 6.0f) == 0);
    CHECK(GridFirst(grid, items, Vector2(123, 100), 6.0f) == -1);
    items[0].alive = false;
    CHECK(GridFirst(grid, items, Vector2(100, 100), 6.0f) == -1);
}

void TestFirstInOrder()
{

    SpatialGrid grid;
    grid.Init(kW, kH, 39.0f);
    std::vector<Item> items;
    for (int i = 0; i < 5; ++i) items.push_back({ Vector2(300, 300), 16.0f, true });
    BuildGrid(grid, items);
    CHECK(GridFirst(grid, items, Vector2(300, 300), 6.0f) == 0);
    items[0].alive = false; items[1].alive = false;
    CHECK(GridFirst(grid, items, Vector2(300, 300), 6.0f) == 2);

    items.clear();
    items.push_back({ Vector2(321, 300), 16.0f, true });
    items.push_back({ Vector2(300, 300), 16.0f, true });
    BuildGrid(grid, items);
    CHECK(GridFirst(grid, items, Vector2(310, 300), 6.0f) == 0);
}

void TestBoundaryDistance()
{

    const float reach = 6.0f + 32.0f;
    SpatialGrid grid;
    grid.Init(kW, kH, reach + 1.0f);
    bool allFound = true;
    for (int dx = -2; dx <= 2; ++dx) {
        for (int dy = -2; dy <= 2; ++dy) {
            std::vector<Item> items;
            const Vector2 q(500.0f + 3.0f * static_cast<float>(dx), 400.0f + 2.0f * static_cast<float>(dy));
            items.push_back({ Vector2(q.x + reach, q.y), 32.0f, true });
            items.push_back({ Vector2(q.x - reach, q.y), 32.0f, true });
            items.push_back({ Vector2(q.x, q.y + reach), 32.0f, true });
            items.push_back({ Vector2(q.x, q.y - reach), 32.0f, true });
            BuildGrid(grid, items);
            if (GridFirst(grid, items, q, 6.0f) != BruteFirst(items, q, 6.0f)) allFound = false;
        }
    }
    CHECK(allFound);
}

void TestOutOfRangeClamped()
{
    SpatialGrid grid;
    grid.Init(kW, kH, 39.0f);
    std::vector<Item> items;
    items.push_back({ Vector2(2.0f, 2.0f), 16.0f, true });
    items.push_back({ Vector2(kW, kH), 16.0f, true });
    items.push_back({ Vector2(kW, 0.0f), 16.0f, true });
    BuildGrid(grid, items);
    CHECK(GridFirst(grid, items, Vector2(-5.0f, -5.0f), 6.0f) == BruteFirst(items, Vector2(-5.0f, -5.0f), 6.0f));
    CHECK(GridFirst(grid, items, Vector2(kW + 10.0f, kH + 10.0f), 6.0f) == 1);
    CHECK(GridFirst(grid, items, Vector2(kW + 10.0f, -10.0f), 6.0f) == 2);

    CHECK(GridFirst(grid, items, Vector2(-1.0e9f, 5.0e9f), 6.0f) == -1);
    CHECK(GridFirst(grid, items, Vector2(3.0e38f, -3.0e38f), 6.0f) == -1);
}

void TestRandomAgainstBruteForce()
{
    struct Scenario { std::uint32_t seed; int count; float spread; };
    const Scenario scenarios[] = {
        { 1, 50, 1.0f }, { 2, 500, 1.0f }, { 3, 3000, 1.0f }, { 4, 3000, 0.1f }, { 5, 3000, 0.02f },
    };
    const float radii[] = { 8.0f, 16.0f, 32.0f };
    const float reach = 6.0f + 32.0f;

    int mismatches = 0;
    int queries = 0;
    for (const Scenario& sc : scenarios) {
        std::mt19937 rng(sc.seed);
        std::uniform_real_distribution<float> ux(0.0f, kW * sc.spread), uy(0.0f, kH * sc.spread);
        std::uniform_int_distribution<int> ur(0, 2), alive(0, 9);
        std::uniform_real_distribution<float> qx(-100.0f, kW + 100.0f), qy(-100.0f, kH + 100.0f);

        std::vector<Item> items;
        for (int i = 0; i < sc.count; ++i) {
            items.push_back({ Vector2(ux(rng), uy(rng)), radii[ur(rng)], alive(rng) != 0 });
        }
        for (float cell : { reach + 1.0f, 64.0f, 100.0f }) {
            SpatialGrid grid;
            grid.Init(kW, kH, cell);
            BuildGrid(grid, items);
            for (int q = 0; q < 2000; ++q) {
                const Vector2 pos(qx(rng), qy(rng));
                ++queries;
                if (GridFirst(grid, items, pos, 6.0f) != BruteFirst(items, pos, 6.0f)) ++mismatches;
            }
        }
    }
    std::printf("  (무작위 질의 %d건, 불일치 %d건)\n", queries, mismatches);
    CHECK(mismatches == 0);
}

void TestFewerChecksThanBruteForce()
{
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> ux(0.0f, kW), uy(0.0f, kH);
    std::vector<Item> items;
    for (int i = 0; i < 5000; ++i) items.push_back({ Vector2(ux(rng), uy(rng)), 16.0f, true });
    SpatialGrid grid;
    grid.Init(kW, kH, 23.0f);
    BuildGrid(grid, items);

    int gridChecks = 0;
    int bruteChecks = 0;
    for (int q = 0; q < 500; ++q) {
        const Vector2 pos(ux(rng), uy(rng));
        GridFirst(grid, items, pos, 6.0f, &gridChecks);
        BruteFirst(items, pos, 6.0f, &bruteChecks);
    }
    std::printf("  (그리드 검사 %d회 vs 전수 검사 %d회)\n", gridChecks, bruteChecks);
    CHECK(gridChecks * 10 < bruteChecks);
}

void TestRebuild()
{
    SpatialGrid grid;
    grid.Init(kW, kH, 39.0f);
    std::vector<Item> items;
    for (int i = 0; i < 100; ++i) items.push_back({ Vector2(100.0f + static_cast<float>(i), 100.0f), 16.0f, true });
    BuildGrid(grid, items);
    CHECK(GridFirst(grid, items, Vector2(150, 100), 6.0f) >= 0);
    items.resize(3);
    BuildGrid(grid, items);
    CHECK(GridFirst(grid, items, Vector2(150, 100), 6.0f) == BruteFirst(items, Vector2(150, 100), 6.0f));
    items.clear();
    BuildGrid(grid, items);
    CHECK(GridFirst(grid, items, Vector2(150, 100), 6.0f) == -1);
}

}

int main()
{
    TestEmptyAndSingle();
    TestFirstInOrder();
    TestBoundaryDistance();
    TestOutOfRangeClamped();
    TestRandomAgainstBruteForce();
    TestFewerChecksThanBruteForce();
    TestRebuild();

    std::printf("grid_tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
