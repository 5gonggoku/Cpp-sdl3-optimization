#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "data/DataManager.h"
#include "gameplay/Enemy.h"
#include "gameplay/WaveMath.h"
#include "gameplay/World.h"

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

constexpr float kDt = 1.0f / 60.0f;
const std::string kDataDir = GAME_DATA_DIR;

Vector2 ScriptedMoveDir(int tick)
{
    switch ((tick / 120) % 4) {
        case 0: return { 1.0f, 0.0f };
        case 1: return { 0.0f, 1.0f };
        case 2: return { -1.0f, 0.0f };
        default: return { 0.0f, -1.0f };
    }
}

int CountArchetype(const World& w, int archetypeId)
{
    int n = 0;
    for (const auto& e : w.GetEnemies()) n += (e->ArchetypeId() == archetypeId) ? 1 : 0;
    return n;
}

int CountWave(const World& w, int waveIndex)
{
    int n = 0;
    for (const auto& e : w.GetEnemies()) n += (e->WaveIndex() == waveIndex) ? 1 : 0;
    return n;
}

void TestDistributeByWeight()
{
    CHECK((DistributeByWeight(10000, { 12, 6, 2 }) == std::vector<int>{ 6000, 3000, 1000 }));
    CHECK((DistributeByWeight(7, { 12, 6, 2 }) == std::vector<int>{ 5, 2, 0 }));
    CHECK((DistributeByWeight(10, { 1, 1, 1 }) == std::vector<int>{ 4, 3, 3 }));
    CHECK((DistributeByWeight(7, { 20 }) == std::vector<int>{ 7 }));
    CHECK((DistributeByWeight(3, { 0, 5, 0 }) == std::vector<int>{ 0, 3, 0 }));
    CHECK((DistributeByWeight(0, { 12, 6, 2 }) == std::vector<int>{ 0, 0, 0 }));
    CHECK((DistributeByWeight(100, { 0, 0 }) == std::vector<int>{ 0, 0 }));
    CHECK(DistributeByWeight(5, {}).empty());

    for (int total = 0; total <= 50; ++total) {
        const std::vector<int> s = DistributeByWeight(total, { 12, 6, 2 });
        CHECK(s[0] + s[1] + s[2] == total);
    }
}

void TestEnemyDamage()
{
    EnemyData brute;
    brute.id = 3;
    brute.hp = 3;
    brute.radius = 32.0f;

    Enemy e(Vector2(0.0f, 0.0f), &brute, 2);
    CHECK(e.Hp() == 3 && e.alive);
    CHECK(e.WaveIndex() == 2 && e.ArchetypeId() == 3 && e.Radius() == 32.0f);
    CHECK(!e.TakeDamage(1) && e.alive && e.Hp() == 2);
    CHECK(!e.TakeDamage(1) && e.alive && e.Hp() == 1);
    CHECK(e.TakeDamage(1) && !e.alive && e.Hp() == 0);

    Enemy overkill(Vector2(0.0f, 0.0f), &brute, 0);
    CHECK(overkill.TakeDamage(10) && !overkill.alive);

    EnemyData grunt;
    grunt.hp = 1;
    Enemy g(Vector2(0.0f, 0.0f), &grunt, 0);
    CHECK(g.TakeDamage(1) && !g.alive);
}

void TestLegacyChecksums()
{
    DataManager data;
    std::string err;
    CHECK(data.Load(kDataDir + "/legacy_v1", err));

    struct Case { int n; std::uint64_t expected; };
    const Case cases[] = { { 500, 0x6149c9ac4e09e520ull }, { 2000, 0xbac66c4430501d06ull } };
    for (const Case& c : cases) {
        WorldConfig cfg;
        cfg.enemyCount = c.n;
        World world(data, cfg);
        CHECK(world.TargetEnemyCount() == c.n);
        for (int tick = 0; tick < 3900; ++tick) world.Update(kDt, ScriptedMoveDir(tick));
        if (world.ComputeStateHash() != c.expected) {
            std::printf("  N=%d 기대 %016llx 실제 %016llx\n", c.n, static_cast<unsigned long long>(c.expected),
                        static_cast<unsigned long long>(world.ComputeStateHash()));
        }
        CHECK(world.ComputeStateHash() == c.expected);
        CHECK(static_cast<int>(world.GetEnemies().size()) == c.n);
    }

    WorldConfig cfg;
    World world(data, cfg);
    CHECK(world.TargetEnemyCount() == 20);
    for (int tick = 0; tick < 300; ++tick) world.Update(kDt, ScriptedMoveDir(tick));
    CHECK(world.GetEnemies().size() == 20);
}

void TestMultiWaveMaintain()
{
    DataManager data;
    std::string err;
    CHECK(data.Load(kDataDir, err));

    WorldConfig cfg;
    World world(data, cfg);
    CHECK(world.TargetEnemyCount() == 20);
    CHECK(world.GetEnemies().size() == 20);

    bool orderOk = true;
    for (std::size_t i = 0; i < world.GetEnemies().size(); ++i) {
        const int expected = (i < 12) ? 1 : (i < 18) ? 2 : 3;
        if (world.GetEnemies()[i]->ArchetypeId() != expected) orderOk = false;
    }
    CHECK(orderOk);

    bool countsOk = true;
    for (int tick = 0; tick < 1800; ++tick) {
        world.Update(kDt, ScriptedMoveDir(tick));
        if (CountWave(world, 0) != 12 || CountWave(world, 1) != 6 || CountWave(world, 2) != 2) countsOk = false;
        if (CountArchetype(world, 1) != 12 || CountArchetype(world, 2) != 6 || CountArchetype(world, 3) != 2) countsOk = false;
    }
    CHECK(countsOk);
    CHECK(world.TotalBulletHits() > 0);

    WorldConfig big;
    big.enemyCount = 1000;
    World w2(data, big);
    CHECK(w2.TargetEnemyCount() == 1000);
    CHECK(CountWave(w2, 0) == 600 && CountWave(w2, 1) == 300 && CountWave(w2, 2) == 100);

    WorldConfig odd;
    odd.enemyCount = 7;
    World w3(data, odd);
    CHECK(w3.TargetEnemyCount() == 7);
    CHECK(CountWave(w3, 0) == 5 && CountWave(w3, 1) == 2 && CountWave(w3, 2) == 0);

    WorldConfig zero;
    zero.enemyCount = 0;
    World w4(data, zero);
    CHECK(w4.GetEnemies().empty());
    for (int tick = 0; tick < 60; ++tick) w4.Update(kDt, ScriptedMoveDir(tick));
    CHECK(w4.GetEnemies().empty());
}

void TestDeterminism()
{
    DataManager data;
    std::string err;
    CHECK(data.Load(kDataDir, err));
    WorldConfig cfg;
    cfg.enemyCount = 300;
    World a(data, cfg);
    World b(data, cfg);
    for (int tick = 0; tick < 900; ++tick) {
        a.Update(kDt, ScriptedMoveDir(tick));
        b.Update(kDt, ScriptedMoveDir(tick));
    }
    CHECK(a.ComputeStateHash() == b.ComputeStateHash());
}

void TestHpAndDamage()
{
    const std::string enemies = "{\"enemies\":[{\"id\":1,\"name\":\"tank\",\"speed\":0,\"radius\":8,\"hp\":3}]}";
    const std::string waves = "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":1,\"mode\":\"maintain\"}]}";
    DataManager data;
    std::string err;
    CHECK(data.LoadFromText(enemies, waves, err));

    WorldConfig cfg;
    cfg.bulletsPerVolley = 360;
    cfg.bulletLifetime = 4.0f;
    World world(data, cfg);

    bool invariantOk = true;
    for (int tick = 0; tick < 900; ++tick) {
        world.Update(kDt, Vector2(0.0f, 0.0f));
        const int hp = world.GetEnemies()[0]->Hp();

        if (static_cast<int>(world.TotalBulletHits() % 3) != (3 - hp) % 3) invariantOk = false;
        if (world.GetEnemies().size() != 1) invariantOk = false;
    }
    CHECK(world.TotalPlayerHits() == 0);
    CHECK(invariantOk);
    CHECK(world.TotalBulletHits() >= 3);

    WorldConfig strong;
    strong.bulletsPerVolley = 360;
    strong.bulletLifetime = 4.0f;
    strong.bulletDamage = 3;
    World w2(data, strong);
    bool alwaysFull = true;
    for (int tick = 0; tick < 900; ++tick) {
        w2.Update(kDt, Vector2(0.0f, 0.0f));
        if (w2.GetEnemies()[0]->Hp() != 3) alwaysFull = false;
    }

    CHECK(alwaysFull);
    CHECK(w2.TotalBulletHits() >= 1);
}

void TestTimedSpawnTiming()
{

    const std::string enemies = "{\"enemies\":[{\"id\":1,\"name\":\"slow\",\"speed\":1,\"radius\":1,\"hp\":1000}]}";
    const std::string waves =
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":4,\"mode\":\"timed\",\"start\":1.0,\"interval\":0.5,\"repeat\":3}]}";
    DataManager data;
    std::string err;
    CHECK(data.LoadFromText(enemies, waves, err));

    WorldConfig cfg;
    cfg.enemyCount = 50;
    World world(data, cfg);
    CHECK(world.TargetEnemyCount() == 0);
    CHECK(world.GetEnemies().empty());

    bool timingOk = true;
    for (int call = 1; call <= 150; ++call) {
        world.Update(kDt, Vector2(0.0f, 0.0f));
        const std::size_t expected = (call <= 60) ? 0 : (call <= 90) ? 4 : (call <= 120) ? 8 : 12;
        if (world.GetEnemies().size() != expected) {
            timingOk = false;
            std::printf("  call %d: 기대 %zu 실제 %zu\n", call, expected, world.GetEnemies().size());
            break;
        }
    }
    CHECK(timingOk);
    CHECK(world.TotalPlayerHits() == 0);
}

void TestTimedDoesNotRespawn()
{
    const std::string enemies = "{\"enemies\":[{\"id\":1,\"name\":\"grunt\",\"speed\":80,\"radius\":16,\"hp\":1}]}";
    const std::string waves =
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":3,\"mode\":\"maintain\"},"
        "{\"id\":2,\"archetype\":1,\"count\":4,\"mode\":\"timed\",\"start\":0,\"interval\":1,\"repeat\":1}]}";
    DataManager data;
    std::string err;
    CHECK(data.LoadFromText(enemies, waves, err));

    WorldConfig cfg;
    World world(data, cfg);
    CHECK(world.TargetEnemyCount() == 3);

    int maxTimed = 0;
    int prevTimed = 0;
    bool neverIncreasesAfterSpawn = true;
    bool maintainAlwaysThree = true;
    for (int tick = 0; tick < 3000; ++tick) {
        world.Update(kDt, Vector2(0.0f, 0.0f));
        const int timed = CountWave(world, 1);
        const int maintain = CountWave(world, 0);
        if (maintain != 3) maintainAlwaysThree = false;
        if (tick >= 1 && timed > prevTimed) neverIncreasesAfterSpawn = false;
        if (timed > maxTimed) maxTimed = timed;
        prevTimed = timed;
    }
    CHECK(maxTimed == 4);
    CHECK(neverIncreasesAfterSpawn);
    CHECK(prevTimed == 0);
    CHECK(maintainAlwaysThree);
}

void TestTimedWithOverride()
{
    DataManager data;
    std::string err;
    CHECK(data.Load(kDataDir + "/demo_timed", err));

    WorldConfig cfg;
    cfg.enemyCount = 100;
    World world(data, cfg);
    CHECK(world.TargetEnemyCount() == 100);
    CHECK(world.GetEnemies().size() == 100);

    bool grew = false;
    for (int tick = 0; tick < 400; ++tick) {
        world.Update(kDt, ScriptedMoveDir(tick));
        if (CountArchetype(world, 2) > 0) grew = true;
    }
    CHECK(grew);
}

}

void TestNaiveGridCrossCheck()
{
    struct Case { const char* dataSub; int n; float cell; std::uint32_t seed; };
    const Case cases[] = {
        { "",              500,  0.0f,   12345 },
        { "",              2000, 0.0f,   12345 },
        { "",              2000, 64.0f,  777 },
        { "",              2000, 100.0f, 31337 },
        { "/legacy_v1",    2000, 0.0f,   12345 },
        { "/legacy_v1",    500,  50.0f,  99 },
        { "/demo_timed",   -1,   0.0f,   12345 },
    };

    for (const Case& c : cases) {
        DataManager data;
        std::string err;
        CHECK(data.Load(kDataDir + c.dataSub, err));

        WorldConfig a;
        a.enemyCount = c.n;
        a.seed = c.seed;
        a.gridCellSize = c.cell;
        a.collision = CollisionMode::Naive;
        WorldConfig b = a;
        b.collision = CollisionMode::Grid;

        World naive(data, a);
        World grid(data, b);

        bool stateOk = true, hitsOk = true, countOk = true;
        std::uint64_t naiveChecks = 0, gridChecks = 0;
        const int ticks = (c.n == -1) ? 1200 : 1500;
        for (int tick = 0; tick < ticks; ++tick) {
            naive.Update(kDt, ScriptedMoveDir(tick));
            grid.Update(kDt, ScriptedMoveDir(tick));
            if (naive.ComputeStateHash() != grid.ComputeStateHash()) stateOk = false;
            if (naive.HitPairHash() != grid.HitPairHash() || naive.TotalBulletHits() != grid.TotalBulletHits()) hitsOk = false;
            if (naive.GetEnemies().size() != grid.GetEnemies().size() ||
                naive.GetBullets().size() != grid.GetBullets().size()) countOk = false;
            naiveChecks += naive.GetLastTickStats().collisionChecks;
            gridChecks += grid.GetLastTickStats().collisionChecks;
        }
        if (!stateOk || !hitsOk || !countOk) {
            std::printf("  교차 검증 실패: data=%s N=%d cell=%.0f seed=%u\n", c.dataSub, c.n, c.cell, c.seed);
        }
        CHECK(stateOk);
        CHECK(hitsOk);
        CHECK(countOk);
        CHECK(naive.TotalBulletHits() > 0);
        CHECK(grid.GetCollisionMode() == CollisionMode::Grid && naive.GetCollisionMode() == CollisionMode::Naive);
        CHECK(naive.GridCellSize() == 0.0f && grid.GridCellSize() > 0.0f);
        if (c.n >= 2000) CHECK(gridChecks * 2 < naiveChecks);
    }

    DataManager data;
    std::string err;
    CHECK(data.Load(kDataDir, err));
    WorldConfig tiny;
    tiny.enemyCount = 100;
    tiny.gridCellSize = 5.0f;
    World w1(data, tiny);
    CHECK(w1.GridCellSize() == 39.0f);
    WorldConfig big = tiny;
    big.gridCellSize = 80.0f;
    World w2(data, big);
    CHECK(w2.GridCellSize() == 80.0f);
    World w3(data, WorldConfig{});
    CHECK(w3.GridCellSize() == 39.0f);
}

int main()
{
    TestDistributeByWeight();
    TestEnemyDamage();
    TestLegacyChecksums();
    TestMultiWaveMaintain();
    TestDeterminism();
    TestHpAndDamage();
    TestTimedSpawnTiming();
    TestTimedDoesNotRespawn();
    TestTimedWithOverride();
    TestNaiveGridCrossCheck();

    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
