#pragma once
#include <string>

struct EnemyData {
    int id = 0;
    std::string name;
    float speed = 0.0f;
    float radius = 0.0f;
    int hp = 1;
};

enum class WaveMode {
    Maintain,
    Timed,
};

struct WaveData {
    int id = 0;
    int archetype = 0;
    int count = 0;
    WaveMode mode = WaveMode::Maintain;

    float start = 0.0f;
    float interval = 0.0f;
    int repeat = 0;
};
