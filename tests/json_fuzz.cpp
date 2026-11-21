#include <cstddef>
#include <cstdint>
#include <string>

#include "data/DataManager.h"
#include "data/Json.h"

namespace {

const char* const kValidEnemies = R"({
  "enemies": [
    { "id": 1, "name": "grunt", "speed": 80, "radius": 16, "hp": 1 },
    { "id": 2, "name": "swarm", "speed": 140, "radius": 8, "hp": 1 },
    { "id": 3, "name": "brute", "speed": 40, "radius": 32, "hp": 3 }
  ]
})";

const char* const kValidWaves = R"({
  "waves": [
    { "id": 1, "archetype": 1, "count": 12, "mode": "maintain" },
    { "id": 2, "archetype": 2, "count": 6, "mode": "maintain" },
    { "id": 3, "archetype": 3, "count": 2, "mode": "maintain" }
  ]
})";

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string text(reinterpret_cast<const char*>(data), size);

    JsonValue value;
    JsonError error;
    ParseJson(text, value, error);

    std::string message;
    {
        DataManager withBadEnemies;
        withBadEnemies.LoadFromText(text, kValidWaves, message);
    }
    {
        DataManager withBadWaves;
        withBadWaves.LoadFromText(kValidEnemies, text, message);
    }
    return 0;
}
