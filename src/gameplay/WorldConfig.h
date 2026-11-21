#pragma once
#include <cstdint>

enum class CollisionMode {
    Naive,
    Grid,
};

struct WorldConfig {
    CollisionMode collision = CollisionMode::Grid;
    float gridCellSize = 0.0f;
    int enemyCount = -1;

    int bulletsPerVolley = 8;
    float bulletLifetime = 2.0f;
    int bulletDamage = 1;
    std::uint32_t seed = 12345;
};
