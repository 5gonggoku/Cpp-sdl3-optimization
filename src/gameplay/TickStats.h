#pragma once
#include <cstdint>

struct TickStats {
    double moveMs = 0.0;
    double collisionMs = 0.0;
    double gridBuildMs = 0.0;
    double cleanupMs = 0.0;
    double totalMs = 0.0;
    std::uint64_t collisionChecks = 0;
};
