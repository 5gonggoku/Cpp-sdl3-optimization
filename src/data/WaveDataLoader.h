#pragma once
#include <vector>

#include "data/DataTypes.h"
#include "data/ILoader.h"

class WaveDataLoader : public ILoader<int, WaveData> {
public:
    bool FromJson(const JsonValue& root, JsonError& err) override;
    bool MakeDict(std::map<int, WaveData>& out, JsonError& err) override;

private:
    std::vector<WaveData> items_;
};
