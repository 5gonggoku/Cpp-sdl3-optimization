#include "data/WaveDataLoader.h"

#include "data/JsonFields.h"

namespace {

bool Reject(const JsonValue& rec, JsonError& err, const std::string& message)
{
    err.line = rec.line;
    err.column = rec.column;
    err.message = message;
    return false;
}

}

bool WaveDataLoader::FromJson(const JsonValue& root, JsonError& err)
{
    items_.clear();
    const JsonValue* list = RequireArrayField(root, "waves", err);
    if (!list) return false;

    for (const JsonValue& rec : list->array) {
        WaveData d;
        std::string mode;
        if (!ReadInt(rec, "id", d.id, err)) return false;
        if (!ReadInt(rec, "archetype", d.archetype, err)) return false;
        if (!ReadInt(rec, "count", d.count, err)) return false;
        if (!ReadString(rec, "mode", mode, err)) return false;

        if (d.count < 0) return Reject(rec, err, "count는 0 이상이어야 함");

        if (mode == "maintain") {
            d.mode = WaveMode::Maintain;
        } else if (mode == "timed") {
            d.mode = WaveMode::Timed;

            if (!ReadFloat(rec, "start", d.start, err)) return false;
            if (!ReadFloat(rec, "interval", d.interval, err)) return false;
            if (!ReadInt(rec, "repeat", d.repeat, err)) return false;
            if (d.start < 0.0f) return Reject(rec, err, "start는 0 이상이어야 함");
            if (d.interval <= 0.0f) return Reject(rec, err, "interval은 0보다 커야 함");
            if (d.repeat < 1) return Reject(rec, err, "repeat는 1 이상이어야 함");
        } else {
            return Reject(rec, err, "알 수 없는 mode: " + mode + " (지원: maintain, timed)");
        }
        items_.push_back(d);
    }
    return true;
}

bool WaveDataLoader::MakeDict(std::map<int, WaveData>& out, JsonError& err)
{
    return MoveItemsToDict(items_, out, [](const WaveData& d) { return d.id; }, err);
}
