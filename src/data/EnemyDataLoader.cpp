#include "data/EnemyDataLoader.h"

#include "data/JsonFields.h"

bool EnemyDataLoader::FromJson(const JsonValue& root, JsonError& err)
{
    items_.clear();
    const JsonValue* list = RequireArrayField(root, "enemies", err);
    if (!list) return false;

    for (const JsonValue& rec : list->array) {
        EnemyData d;
        if (!ReadInt(rec, "id", d.id, err)) return false;
        if (!ReadString(rec, "name", d.name, err)) return false;
        if (!ReadFloat(rec, "speed", d.speed, err)) return false;
        if (!ReadFloat(rec, "radius", d.radius, err)) return false;
        if (!ReadInt(rec, "hp", d.hp, err)) return false;

        if (d.hp < 1) { err.line = rec.line; err.column = rec.column; err.message = "hp는 1 이상이어야 함"; return false; }
        if (d.speed < 0.0f) { err.line = rec.line; err.column = rec.column; err.message = "speed는 0 이상이어야 함"; return false; }
        if (d.radius <= 0.0f) { err.line = rec.line; err.column = rec.column; err.message = "radius는 0보다 커야 함"; return false; }
        items_.push_back(std::move(d));
    }
    return true;
}

bool EnemyDataLoader::MakeDict(std::map<int, EnemyData>& out, JsonError& err)
{
    return MoveItemsToDict(items_, out, [](const EnemyData& d) { return d.id; }, err);
}
