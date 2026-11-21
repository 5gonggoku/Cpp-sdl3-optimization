#pragma once
#include <vector>

#include "data/DataTypes.h"
#include "data/ILoader.h"

class EnemyDataLoader : public ILoader<int, EnemyData> {
public:
    bool FromJson(const JsonValue& root, JsonError& err) override;
    bool MakeDict(std::map<int, EnemyData>& out, JsonError& err) override;

private:
    std::vector<EnemyData> items_;
};
