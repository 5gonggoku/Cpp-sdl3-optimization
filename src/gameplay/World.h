#pragma once
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "data/DataManager.h"
#include "gameplay/Bullet.h"
#include "gameplay/Enemy.h"
#include "gameplay/Player.h"
#include "gameplay/TickStats.h"
#include "gameplay/WorldConfig.h"
#include "math/Vector2.h"
#include "gameplay/Hash.h"
#include "memory/Pool.h"
#include "physics/SpatialGrid.h"

class World {
public:
    static constexpr float kWidth = 1280.0f;
    static constexpr float kHeight = 720.0f;

    static constexpr float kBulletFireInterval = 0.5f;
    static constexpr float kBulletSpeed = 400.0f;

    World(const DataManager& data, const WorldConfig& config);

    void Update(float dt, Vector2 moveDir);

    int TargetEnemyCount() const { return targetEnemyCount_; }
    const Player& GetPlayer() const { return player_; }

    const std::vector<Enemy*>& GetEnemies() const { return enemies_; }
    const std::vector<Bullet*>& GetBullets() const { return bullets_; }

    const TickStats& GetLastTickStats() const { return lastTickStats_; }

    std::uint64_t TotalBulletHits() const { return totalBulletHits_; }
    std::uint64_t TotalPlayerHits() const { return totalPlayerHits_; }
    std::uint64_t TickCount() const { return tickCount_; }

    CollisionMode GetCollisionMode() const { return config_.collision; }
    float GridCellSize() const { return config_.collision == CollisionMode::Grid ? gridCellSize_ : 0.0f; }

    std::uint64_t HitPairHash() const { return hitPairs_.Value(); }

    std::uint64_t ComputeStateHash() const;

private:

    struct WaveRuntime {
        const WaveData* wave = nullptr;
        const EnemyData* archetype = nullptr;
        int target = 0;
        int nextSpawnIndex = 0;
    };

    Vector2 RandomEdgePosition();
    void SpawnEnemy(int waveIndex);
    void SpawnTimedWaves();
    void SpawnBulletVolley();
    void UpdateEnemies(float dt);
    void UpdateBullets(float dt);
    void ResolveCollisions();
    void ResolveBulletsNaive();
    void ResolveBulletsGrid();
    void RegisterHit(std::size_t bulletIndex, std::size_t enemyIndex, Bullet* bullet, Enemy* enemy);
    void CleanupAndRespawn();

    WorldConfig config_;
    std::vector<WaveRuntime> waves_;
    std::vector<int> deadPerWave_;
    int targetEnemyCount_ = 0;
    double elapsedSeconds_ = 0.0;
    Player player_;

    Pool<Enemy> enemyPool_;
    Pool<Bullet> bulletPool_;
    std::vector<Enemy*> enemies_;
    std::vector<Bullet*> bullets_;
    SpatialGrid grid_;
    float gridCellSize_ = 0.0f;
    std::mt19937 rng_;
    float bulletFireTimer_ = 0.0f;

    TickStats lastTickStats_;
    std::uint64_t collisionChecksThisTick_ = 0;
    double gridBuildMsThisTick_ = 0.0;
    Fnv1a hitPairs_;
    std::uint64_t totalBulletHits_ = 0;
    std::uint64_t totalPlayerHits_ = 0;
    std::uint64_t tickCount_ = 0;
};
