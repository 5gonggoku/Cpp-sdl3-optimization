#pragma once
#include <string>

#include "gameplay/WorldConfig.h"

struct CliOptions {
    bool bench = false;
    bool verify = false;
    int enemies = -1;
    int bullets = -1;
    float lifetime = -1.0f;
    long long seed = -1;
    int ticks = 3600;
    int warmup = 300;
    bool collisionSet = false;
    CollisionMode collision = CollisionMode::Grid;
    float cell = -1.0f;
    std::string csvPath;
    std::string dataDir = "data";
    bool dataDirExplicit = false;
};

bool ParseCli(int argc, char** argv, CliOptions& out);

std::string ResolveDataDir(const CliOptions& opt, const char* argv0);

WorldConfig BuildWorldConfig(const CliOptions& opt);
