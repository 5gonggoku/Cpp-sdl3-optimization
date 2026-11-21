#pragma once
#include <cstdint>
#include <map>
#include <string>

#include "data/DataTypes.h"

class DataManager {
public:

    bool Load(const std::string& dir, std::string& errMsg);

    bool LoadFromText(const std::string& enemiesText, const std::string& wavesText, std::string& errMsg,
                      const std::string& enemiesName = "enemies.json",
                      const std::string& wavesName = "waves.json");

    const std::map<int, EnemyData>& Enemies() const { return enemies_; }

    const std::map<int, WaveData>& Waves() const { return waves_; }

    const EnemyData* FindEnemy(int id) const;

    std::uint64_t DataHash() const { return dataHash_; }
    const std::string& Dir() const { return dir_; }

private:
    std::map<int, EnemyData> enemies_;
    std::map<int, WaveData> waves_;
    std::uint64_t dataHash_ = 0;
    std::string dir_;
};
