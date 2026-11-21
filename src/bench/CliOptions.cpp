#include "bench/CliOptions.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace {

const char* MatchValue(const char* arg, const char* name)
{
    const std::size_t n = std::strlen(name);
    if (std::strncmp(arg, name, n) == 0 && arg[n] == '=') return arg + n + 1;
    return nullptr;
}

bool ParseLong(const char* s, long long& out)
{
    char* end = nullptr;
    out = std::strtoll(s, &end, 10);
    return end != s && *end == '\0';
}

bool ParseFloat(const char* s, float& out)
{
    char* end = nullptr;
    out = std::strtof(s, &end);
    return end != s && *end == '\0';
}

}

bool ParseCli(int argc, char** argv, CliOptions& out)
{
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        long long n = 0;
        float f = 0.0f;
        const char* v = nullptr;

        if (std::strcmp(arg, "--bench") == 0) { out.bench = true; continue; }
        if (std::strcmp(arg, "--verify") == 0) { out.verify = true; continue; }

        if ((v = MatchValue(arg, "--enemies")) && ParseLong(v, n) && n >= 0) { out.enemies = static_cast<int>(n); continue; }
        if ((v = MatchValue(arg, "--bullets")) && ParseLong(v, n) && n >= 1) { out.bullets = static_cast<int>(n); continue; }
        if ((v = MatchValue(arg, "--lifetime")) && ParseFloat(v, f) && f > 0.0f) { out.lifetime = f; continue; }
        if ((v = MatchValue(arg, "--seed")) && ParseLong(v, n) && n >= 0) { out.seed = n; continue; }
        if ((v = MatchValue(arg, "--ticks")) && ParseLong(v, n) && n >= 1) { out.ticks = static_cast<int>(n); continue; }
        if ((v = MatchValue(arg, "--warmup")) && ParseLong(v, n) && n >= 0) { out.warmup = static_cast<int>(n); continue; }
        if ((v = MatchValue(arg, "--collision"))) {
            if (std::strcmp(v, "naive") == 0) { out.collision = CollisionMode::Naive; out.collisionSet = true; continue; }
            if (std::strcmp(v, "grid") == 0) { out.collision = CollisionMode::Grid; out.collisionSet = true; continue; }
        }
        if ((v = MatchValue(arg, "--cell")) && ParseFloat(v, f) && f > 0.0f) { out.cell = f; continue; }
        if ((v = MatchValue(arg, "--csv")) && *v) { out.csvPath = v; continue; }
        if ((v = MatchValue(arg, "--data")) && *v) { out.dataDir = v; out.dataDirExplicit = true; continue; }

        std::fprintf(stderr, "잘못된 옵션 또는 값: %s\n", arg);
        return false;
    }
    return true;
}

std::string ResolveDataDir(const CliOptions& opt, const char* argv0)
{
    namespace fs = std::filesystem;
    if (opt.dataDirExplicit) return opt.dataDir;

    std::error_code ec;
    if (fs::exists(fs::path("data") / "enemies.json", ec)) return opt.dataDir;

    if (argv0 != nullptr && *argv0 != '\0') {
        fs::path dir = fs::absolute(fs::path(argv0), ec).parent_path();
        for (int level = 0; level < 8 && !dir.empty(); ++level) {
            const fs::path candidate = dir / "data";
            if (fs::exists(candidate / "enemies.json", ec)) return candidate.string();
            const fs::path parent = dir.parent_path();
            if (parent == dir) break;
            dir = parent;
        }
    }
    return opt.dataDir;
}

WorldConfig BuildWorldConfig(const CliOptions& opt)
{
    WorldConfig cfg;
    if (opt.bench || opt.verify) cfg.enemyCount = 10000;
    if (opt.enemies >= 0) cfg.enemyCount = opt.enemies;
    if (opt.bullets >= 1) cfg.bulletsPerVolley = opt.bullets;
    if (opt.lifetime > 0.0f) cfg.bulletLifetime = opt.lifetime;
    if (opt.seed >= 0) cfg.seed = static_cast<std::uint32_t>(opt.seed);
    if (opt.collisionSet) cfg.collision = opt.collision;
    if (opt.cell > 0.0f) cfg.gridCellSize = opt.cell;
    return cfg;
}
