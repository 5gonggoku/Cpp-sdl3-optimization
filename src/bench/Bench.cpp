#include "bench/Bench.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "bench/EnvInfo.h"
#include "data/DataManager.h"
#include "gameplay/Hash.h"
#include "gameplay/World.h"
#include "memory/AllocCounter.h"

namespace {

constexpr float kFixedDt = 1.0f / 60.0f;
constexpr int kInputPhaseTicks = 120;
constexpr int kVerifyPrintInterval = 600;

Vector2 ScriptedMoveDir(int tick)
{
    switch ((tick / kInputPhaseTicks) % 4) {
        case 0: return { 1.0f, 0.0f };
        case 1: return { 0.0f, 1.0f };
        case 2: return { -1.0f, 0.0f };
        default: return { 0.0f, -1.0f };
    }
}

struct BenchResult {
    double avgMs = 0, p99Ms = 0, maxMs = 0;
    double moveMs = 0, collisionMs = 0, cleanupMs = 0, gridBuildMs = 0;
    const char* collisionName = "naive";
    float cellSize = 0.0f;
    double checksPerTick = 0;
    double allocsPerTick = 0;
    std::uint64_t allocsTotal = 0;
    std::uint64_t checksum = 0;
};

std::string DataHashString(const DataManager& data)
{
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(data.DataHash()));
    return buf;
}

const char* CollisionName(CollisionMode mode) { return mode == CollisionMode::Grid ? "grid" : "naive"; }

void PrintScenario(const CliOptions& opt, const WorldConfig& cfg, int enemyCount, const DataManager& data,
                   const EnvInfo& env, const World& world)
{
    std::printf("[scenario] seed=%u enemies=%d bullets=%d lifetime=%.2f warmup=%d ticks=%d dt=1/60\n",
                cfg.seed, enemyCount, cfg.bulletsPerVolley, cfg.bulletLifetime, opt.warmup, opt.ticks);
    std::printf("[collision] mode=%s cell_size=%.1f\n", CollisionName(world.GetCollisionMode()), world.GridCellSize());
    std::printf("[data] dir=\"%s\" data_hash=%s\n", data.Dir().c_str(), DataHashString(data).c_str());
    std::printf("[env] cpu=\"%s\" ram=%lluMB os=\"%s\" compiler=\"%s\" build=%s commit=%s\n",
                env.cpu.c_str(), static_cast<unsigned long long>(env.ramMB), env.os.c_str(),
                env.compiler.c_str(), env.buildType.c_str(), env.commit.c_str());
}

bool AppendCsv(const char* path, const CliOptions& opt, const WorldConfig& cfg, int enemyCount,
               const DataManager& data, const EnvInfo& env, const BenchResult& r)
{
    std::FILE* f = nullptr;
    if (fopen_s(&f, path, "ab") != 0 || !f) {
        std::fprintf(stderr, "CSV 파일을 열 수 없음: %s\n", path);
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    if (std::ftell(f) == 0) {
        std::fputs("commit,build,cpu,ram_mb,os,compiler,seed,enemies,bullets,lifetime,warmup,ticks,"
                   "checksum,avg_ms,p99_ms,max_ms,move_ms,collision_ms,cleanup_ms,"
                   "checks_per_tick,allocs_per_tick,allocs_total,data_hash,collision,cell_size,grid_build_ms\n", f);
    }
    std::fprintf(f,
                 "%s,%s,\"%s\",%llu,\"%s\",\"%s\",%u,%d,%d,%.2f,%d,%d,"
                 "%016llx,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.1f,%.3f,%llu,%s,%s,%.1f,%.6f\n",
                 env.commit.c_str(), env.buildType.c_str(), env.cpu.c_str(),
                 static_cast<unsigned long long>(env.ramMB), env.os.c_str(), env.compiler.c_str(),
                 cfg.seed, enemyCount, cfg.bulletsPerVolley, cfg.bulletLifetime, opt.warmup, opt.ticks,
                 static_cast<unsigned long long>(r.checksum), r.avgMs, r.p99Ms, r.maxMs, r.moveMs,
                 r.collisionMs, r.cleanupMs, r.checksPerTick, r.allocsPerTick,
                 static_cast<unsigned long long>(r.allocsTotal), DataHashString(data).c_str(), r.collisionName,
                 r.cellSize, r.gridBuildMs);
    std::fclose(f);
    return true;
}

int RunVerify(const CliOptions& opt, World& world)
{
    Fnv1a running;
    const int total = opt.warmup + opt.ticks;
    std::printf("[verify] 매 틱 상태 해시 누적 (틱 시간은 측정하지 않음)\n");
    for (int tick = 0; tick < total; ++tick) {
        world.Update(kFixedDt, ScriptedMoveDir(tick));
        running.AddU64(world.ComputeStateHash());
        if ((tick + 1) % kVerifyPrintInterval == 0) {
            std::printf("[verify] tick %d running=%016llx\n", tick + 1,
                        static_cast<unsigned long long>(running.Value()));
        }
    }
    std::printf("[verify] total_ticks=%d running_checksum=%016llx final_state_checksum=%016llx\n", total,
                static_cast<unsigned long long>(running.Value()),
                static_cast<unsigned long long>(world.ComputeStateHash()));
    return 0;
}

}

int RunBench(const CliOptions& opt)
{
    const WorldConfig cfg = BuildWorldConfig(opt);
    const EnvInfo env = GetEnvInfo();

    DataManager data;
    std::string dataError;
    if (!data.Load(opt.dataDir, dataError)) {
        std::fprintf(stderr, "데이터 로드 실패: %s\n", dataError.c_str());
        return 1;
    }
    World world(data, cfg);
    const int enemyCount = world.TargetEnemyCount();
    PrintScenario(opt, cfg, enemyCount, data, env, world);

    if (opt.verify) return RunVerify(opt, world);

    int tick = 0;
    for (; tick < opt.warmup; ++tick) {
        world.Update(kFixedDt, ScriptedMoveDir(tick));
    }

    std::vector<double> tickMs;
    tickMs.reserve(static_cast<std::size_t>(opt.ticks));
    double sumMove = 0, sumCollision = 0, sumCleanup = 0, sumTotal = 0, sumGridBuild = 0;
    std::uint64_t sumChecks = 0;

    const std::uint64_t allocBefore = g_allocCount;
    for (int i = 0; i < opt.ticks; ++i, ++tick) {
        world.Update(kFixedDt, ScriptedMoveDir(tick));
        const TickStats& s = world.GetLastTickStats();
        tickMs.push_back(s.totalMs);
        sumMove += s.moveMs;
        sumCollision += s.collisionMs;
        sumGridBuild += s.gridBuildMs;
        sumCleanup += s.cleanupMs;
        sumTotal += s.totalMs;
        sumChecks += s.collisionChecks;
    }
    const std::uint64_t allocAfter = g_allocCount;

    std::vector<double> sorted = tickMs;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t n = sorted.size();
    const std::size_t p99Index = (n * 99 + 99) / 100 - 1;

    BenchResult r;
    const double ticks = static_cast<double>(opt.ticks);
    r.avgMs = sumTotal / ticks;
    r.p99Ms = sorted[p99Index];
    r.maxMs = sorted.back();
    r.moveMs = sumMove / ticks;
    r.collisionMs = sumCollision / ticks;
    r.cleanupMs = sumCleanup / ticks;
    r.gridBuildMs = sumGridBuild / ticks;
    r.collisionName = CollisionName(world.GetCollisionMode());
    r.cellSize = world.GridCellSize();
    r.checksPerTick = static_cast<double>(sumChecks) / ticks;
    r.allocsTotal = allocAfter - allocBefore;
    r.allocsPerTick = static_cast<double>(r.allocsTotal) / ticks;
    r.checksum = world.ComputeStateHash();

    std::printf("[result] tick_ms avg=%.6f p99=%.6f max=%.6f\n", r.avgMs, r.p99Ms, r.maxMs);
    std::printf("[result] section_ms move=%.6f collision=%.6f cleanup=%.6f\n", r.moveMs, r.collisionMs,
                r.cleanupMs);
    std::printf("[result] grid_build_ms=%.6f (collision 구간에 포함)\n", r.gridBuildMs);
    std::printf("[result] collision_checks_per_tick=%.1f\n", r.checksPerTick);
    std::printf("[result] heap_allocs_per_tick=%.3f (total=%llu in %d ticks)\n", r.allocsPerTick,
                static_cast<unsigned long long>(r.allocsTotal), opt.ticks);
    std::printf("[result] checksum=%016llx\n", static_cast<unsigned long long>(r.checksum));

    if (!opt.csvPath.empty() && !AppendCsv(opt.csvPath.c_str(), opt, cfg, enemyCount, data, env, r)) return 1;
    return 0;
}
