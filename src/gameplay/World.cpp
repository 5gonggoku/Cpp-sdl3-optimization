#include "gameplay/World.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "gameplay/Hash.h"
#include "gameplay/WaveMath.h"

namespace {
constexpr float kTwoPi = 6.2831853f;

using Clock = std::chrono::steady_clock;

double ElapsedMs(Clock::time_point from, Clock::time_point to)
{
    return std::chrono::duration<double, std::milli>(to - from).count();
}

inline bool BulletOverlapsEnemy(const Vector2& bulletPos, const Enemy& enemy)
{
    const Vector2 diff = bulletPos - enemy.position;
    const float radiusSum = Bullet::kRadius + enemy.Radius();
    return diff.LengthSq() <= radiusSum * radiusSum;
}
}

World::World(const DataManager& data, const WorldConfig& config)
    : config_(config), rng_(config.seed)
{

    for (const auto& entry : data.Waves()) {
        WaveRuntime rt;
        rt.wave = &entry.second;
        rt.archetype = data.FindEnemy(entry.second.archetype);
        waves_.push_back(rt);
    }
    deadPerWave_.assign(waves_.size(), 0);

    std::vector<int> weights(waves_.size(), 0);
    for (std::size_t i = 0; i < waves_.size(); ++i) {
        if (waves_[i].wave->mode == WaveMode::Maintain) weights[i] = waves_[i].wave->count;
    }
    const std::vector<int> targets =
        config_.enemyCount >= 0 ? DistributeByWeight(config_.enemyCount, weights) : weights;
    for (std::size_t i = 0; i < waves_.size(); ++i) {
        waves_[i].target = targets[i];
        targetEnemyCount_ += targets[i];
    }

    float maxEnemyRadius = 0.0f;
    for (const auto& entry : data.Enemies()) maxEnemyRadius = std::max(maxEnemyRadius, entry.second.radius);
    const float minCellSize = Bullet::kRadius + maxEnemyRadius + 1.0f;
    gridCellSize_ = std::max(config_.gridCellSize, minCellSize);
    if (config_.collision == CollisionMode::Grid) grid_.Init(kWidth, kHeight, gridCellSize_);

    for (std::size_t i = 0; i < waves_.size(); ++i) {
        for (int k = 0; k < waves_[i].target; ++k) SpawnEnemy(static_cast<int>(i));
    }
}

void World::Update(float dt, Vector2 moveDir)
{
    collisionChecksThisTick_ = 0;
    gridBuildMsThisTick_ = 0.0;
    const auto t0 = Clock::now();

    player_.Update(dt, moveDir);
    player_.position.x = std::clamp(player_.position.x, 0.0f, kWidth - Player::kSize);
    player_.position.y = std::clamp(player_.position.y, 0.0f, kHeight - Player::kSize);

    SpawnTimedWaves();
    UpdateEnemies(dt);
    UpdateBullets(dt);

    bulletFireTimer_ += dt;
    if (bulletFireTimer_ >= kBulletFireInterval) {
        bulletFireTimer_ -= kBulletFireInterval;
        SpawnBulletVolley();
    }
    const auto t1 = Clock::now();

    ResolveCollisions();
    const auto t2 = Clock::now();

    CleanupAndRespawn();
    const auto t3 = Clock::now();

    ++tickCount_;
    elapsedSeconds_ += static_cast<double>(dt);
    lastTickStats_.moveMs = ElapsedMs(t0, t1);
    lastTickStats_.collisionMs = ElapsedMs(t1, t2);
    lastTickStats_.gridBuildMs = gridBuildMsThisTick_;
    lastTickStats_.cleanupMs = ElapsedMs(t2, t3);
    lastTickStats_.totalMs = ElapsedMs(t0, t3);
    lastTickStats_.collisionChecks = collisionChecksThisTick_;
}

std::uint64_t World::ComputeStateHash() const
{
    Fnv1a h;
    h.AddFloat(player_.position.x);
    h.AddFloat(player_.position.y);
    for (const Enemy* enemy : enemies_) {
        h.AddFloat(enemy->position.x);
        h.AddFloat(enemy->position.y);
    }
    for (const Bullet* bullet : bullets_) {
        h.AddFloat(bullet->position.x);
        h.AddFloat(bullet->position.y);
    }
    h.AddU64(enemies_.size());
    h.AddU64(bullets_.size());
    h.AddU64(totalBulletHits_);
    h.AddU64(totalPlayerHits_);
    h.AddU64(tickCount_);
    return h.Value();
}

Vector2 World::RandomEdgePosition()
{
    std::uniform_int_distribution<int> edgeDist(0, 3);
    std::uniform_real_distribution<float> xDist(0.0f, kWidth);
    std::uniform_real_distribution<float> yDist(0.0f, kHeight);

    switch (edgeDist(rng_)) {
        case 0: return { xDist(rng_), 0.0f };
        case 1: return { xDist(rng_), kHeight };
        case 2: return { 0.0f, yDist(rng_) };
        default: return { kWidth, yDist(rng_) };
    }
}

void World::SpawnEnemy(int waveIndex)
{
    enemies_.push_back(enemyPool_.Acquire(RandomEdgePosition(), waves_[waveIndex].archetype, waveIndex));
}

void World::SpawnTimedWaves()
{

    for (std::size_t i = 0; i < waves_.size(); ++i) {
        WaveRuntime& rt = waves_[i];
        const WaveData& wave = *rt.wave;
        if (wave.mode != WaveMode::Timed) continue;
        while (rt.nextSpawnIndex < wave.repeat &&
               elapsedSeconds_ >= static_cast<double>(wave.start) +
                                      static_cast<double>(rt.nextSpawnIndex) * static_cast<double>(wave.interval)) {
            for (int k = 0; k < wave.count; ++k) SpawnEnemy(static_cast<int>(i));
            ++rt.nextSpawnIndex;
        }
    }
}

void World::SpawnBulletVolley()
{
    const Vector2 origin = player_.position + Vector2(Player::kSize * 0.5f, Player::kSize * 0.5f);
    for (int i = 0; i < config_.bulletsPerVolley; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(config_.bulletsPerVolley);
        const Vector2 velocity{ std::cos(angle) * kBulletSpeed, std::sin(angle) * kBulletSpeed };
        bullets_.push_back(bulletPool_.Acquire(origin, velocity, config_.bulletLifetime));
    }
}

void World::UpdateEnemies(float dt)
{
    const Vector2 playerCenter = player_.position + Vector2(Player::kSize * 0.5f, Player::kSize * 0.5f);
    for (Enemy* enemy : enemies_) {
        enemy->SetTarget(playerCenter);
        enemy->Update(dt);
    }
}

void World::UpdateBullets(float dt)
{
    for (Bullet* bullet : bullets_) {
        bullet->Update(dt);
    }
}

void World::RegisterHit(std::size_t bulletIndex, std::size_t enemyIndex, Bullet* bullet, Enemy* enemy)
{
    bullet->alive = false;
    enemy->TakeDamage(config_.bulletDamage);
    ++totalBulletHits_;
    hitPairs_.AddU64((static_cast<std::uint64_t>(bulletIndex) << 32) | static_cast<std::uint64_t>(enemyIndex));
}

void World::ResolveBulletsNaive()
{
    for (Bullet* const& bulletSlot : bullets_) {
        Bullet* bullet = bulletSlot;
        if (!bullet->alive) continue;
        for (Enemy* const& enemySlot : enemies_) {
            Enemy* enemy = enemySlot;
            if (!enemy->alive) continue;
            ++collisionChecksThisTick_;
            if (BulletOverlapsEnemy(bullet->position, *enemy)) {

                RegisterHit(static_cast<std::size_t>(&bulletSlot - bullets_.data()),
                            static_cast<std::size_t>(&enemySlot - enemies_.data()), bullet, enemy);
                break;
            }
        }
    }
}

void World::ResolveBulletsGrid()
{
    const auto b0 = Clock::now();
    grid_.Build(enemies_.size(), [this](std::size_t i) { return enemies_[i]->position; });
    gridBuildMsThisTick_ = ElapsedMs(b0, Clock::now());

    for (std::size_t bi = 0; bi < bullets_.size(); ++bi) {
        Bullet* bullet = bullets_[bi];
        if (!bullet->alive) continue;
        const int hit = grid_.FindFirst(bullet->position, [&](int index) {
            const Enemy& enemy = *enemies_[static_cast<std::size_t>(index)];
            if (!enemy.alive) return false;
            ++collisionChecksThisTick_;
            return BulletOverlapsEnemy(bullet->position, enemy);
        });
        if (hit >= 0) RegisterHit(bi, static_cast<std::size_t>(hit), bullet, enemies_[static_cast<std::size_t>(hit)]);
    }
}

void World::ResolveCollisions()
{
    if (config_.collision == CollisionMode::Grid) ResolveBulletsGrid();
    else ResolveBulletsNaive();

    const Vector2 playerCenter = player_.position + Vector2(Player::kSize * 0.5f, Player::kSize * 0.5f);
    const float playerRadius = Player::kSize * 0.5f;
    for (Enemy* enemy : enemies_) {
        if (!enemy->alive) continue;
        const Vector2 diff = enemy->position - playerCenter;
        const float radiusSum = enemy->Radius() + playerRadius;
        if (diff.LengthSq() <= radiusSum * radiusSum) {
            enemy->alive = false;
            ++totalPlayerHits_;
        }
    }
}

void World::CleanupAndRespawn()
{

    std::size_t keptBullets = 0;
    for (std::size_t i = 0; i < bullets_.size(); ++i) {
        Bullet* b = bullets_[i];
        if (b->alive) bullets_[keptBullets++] = b;
        else bulletPool_.Release(b);
    }
    bullets_.resize(keptBullets);

    std::fill(deadPerWave_.begin(), deadPerWave_.end(), 0);
    std::size_t keptEnemies = 0;
    for (std::size_t i = 0; i < enemies_.size(); ++i) {
        Enemy* e = enemies_[i];
        if (e->alive) {
            enemies_[keptEnemies++] = e;
        } else {
            ++deadPerWave_[e->WaveIndex()];
            enemyPool_.Release(e);
        }
    }
    enemies_.resize(keptEnemies);

    for (std::size_t i = 0; i < waves_.size(); ++i) {
        if (waves_[i].wave->mode != WaveMode::Maintain) continue;
        for (int k = 0; k < deadPerWave_[i]; ++k) SpawnEnemy(static_cast<int>(i));
    }
}
