#include "data/DataManager.h"

#include "data/EnemyDataLoader.h"
#include "data/ILoader.h"
#include "data/WaveDataLoader.h"
#include "gameplay/Hash.h"

bool DataManager::Load(const std::string& dir, std::string& errMsg)
{
    dir_ = dir;
    const std::string enemiesPath = dir + "/enemies.json";
    const std::string wavesPath = dir + "/waves.json";

    std::string enemiesText, wavesText;
    if (!ReadTextFile(enemiesPath, enemiesText, errMsg)) return false;
    if (!ReadTextFile(wavesPath, wavesText, errMsg)) return false;
    return LoadFromText(enemiesText, wavesText, errMsg, enemiesPath, wavesPath);
}

bool DataManager::LoadFromText(const std::string& enemiesText, const std::string& wavesText, std::string& errMsg,
                               const std::string& enemiesName, const std::string& wavesName)
{
    enemies_.clear();
    waves_.clear();
    dataHash_ = 0;

    JsonError err;
    if (!LoadTableFromText<EnemyDataLoader>(enemiesText, enemies_, err)) {
        errMsg = FormatError(enemiesName, err);
        return false;
    }
    if (!LoadTableFromText<WaveDataLoader>(wavesText, waves_, err)) {
        errMsg = FormatError(wavesName, err);
        return false;
    }

    if (waves_.empty()) {
        errMsg = wavesName + ": 웨이브가 하나도 없음";
        return false;
    }
    for (const auto& entry : waves_) {
        const WaveData& wave = entry.second;
        if (!FindEnemy(wave.archetype)) {
            errMsg = wavesName + ": 웨이브 " + std::to_string(wave.id) + "가 없는 archetype을 가리킴: " +
                     std::to_string(wave.archetype);
            return false;
        }
    }

    Fnv1a h;
    h.AddBytes(enemiesText.data(), enemiesText.size());
    h.AddBytes(wavesText.data(), wavesText.size());
    dataHash_ = h.Value();
    return true;
}

const EnemyData* DataManager::FindEnemy(int id) const
{
    const auto it = enemies_.find(id);
    return it == enemies_.end() ? nullptr : &it->second;
}
