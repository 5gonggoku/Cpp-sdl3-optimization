#include <cstdio>
#include <string>

#include "data/DataManager.h"
#include "data/EnemyDataLoader.h"
#include "data/Json.h"
#include "data/WaveDataLoader.h"

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_failures;                                                             \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);               \
        }                                                                             \
    } while (0)

bool Parse(const std::string& text, JsonValue& v, JsonError& e) { return ParseJson(text, v, e); }

bool ParseFails(const std::string& text, int line = 0, int col = 0)
{
    JsonValue v;
    JsonError e;
    if (ParseJson(text, v, e)) return false;
    if (e.message.empty()) return false;
    if (line > 0 && (e.line != line || e.column != col)) {
        std::printf("  위치 불일치: 기대 %d:%d 실제 %d:%d (%s)\n", line, col, e.line, e.column, e.message.c_str());
        return false;
    }
    return true;
}

void TestParseValid()
{
    JsonValue v;
    JsonError e;

    CHECK(Parse("null", v, e) && v.type == JsonType::Null);
    CHECK(Parse("true", v, e) && v.type == JsonType::Bool && v.boolean);
    CHECK(Parse("false", v, e) && v.type == JsonType::Bool && !v.boolean);
    CHECK(Parse("  42  ", v, e) && v.type == JsonType::Number && v.number == 42.0);
    CHECK(Parse("-0.5", v, e) && v.number == -0.5);
    CHECK(Parse("1e3", v, e) && v.number == 1000.0);
    CHECK(Parse("2.5E-1", v, e) && v.number == 0.25);
    CHECK(Parse("0", v, e) && v.number == 0.0);

    CHECK(Parse("\"a\\n\\t\\\"\\\\\\/b\"", v, e) && v.string == "a\n\t\"\\/b");
    CHECK(Parse("\"\"", v, e) && v.string.empty());
    CHECK(Parse("\"한글\"", v, e) && v.string == "한글");

    CHECK(Parse("[]", v, e) && v.type == JsonType::Array && v.array.empty());
    CHECK(Parse("{}", v, e) && v.type == JsonType::Object && v.keys.empty());
    CHECK(Parse("[1, [2, 3], {\"a\": null}]", v, e) && v.array.size() == 3 && v.array[1].array.size() == 2 &&
          v.array[2].Find("a") != nullptr);

    CHECK(Parse("{\"x\": 1, \"y\": \"s\", \"z\": [true]}", v, e));
    CHECK(v.Find("x") && v.Find("x")->number == 1.0);
    CHECK(v.Find("y") && v.Find("y")->string == "s");
    CHECK(v.Find("w") == nullptr);
    CHECK(v.keys.size() == 3 && v.keys[0] == "x" && v.keys[2] == "z");

    CHECK(Parse("\xEF\xBB\xBF{\"a\":1}", v, e) && v.Find("a"));

    CHECK(Parse("{\n  \"a\": 5\n}", v, e));
    CHECK(v.Find("a") && v.Find("a")->line == 2 && v.Find("a")->column == 8);
}

void TestParseInvalid()
{
    CHECK(ParseFails(""));
    CHECK(ParseFails("   "));
    CHECK(ParseFails("nul"));
    CHECK(ParseFails("tru"));
    CHECK(ParseFails("TRUE"));
    CHECK(ParseFails("[1,]"));
    CHECK(ParseFails("{\"a\":1,}"));
    CHECK(ParseFails("[1 2]"));
    CHECK(ParseFails("{\"a\" 1}"));
    CHECK(ParseFails("{a:1}"));
    CHECK(ParseFails("{\"a\":1"));
    CHECK(ParseFails("[1"));
    CHECK(ParseFails("\"abc"));
    CHECK(ParseFails("\"a\nb\""));
    CHECK(ParseFails("\"\\x\""));
    CHECK(ParseFails("\"\\u0041\""));
    CHECK(ParseFails("\"abc\\"));
    CHECK(ParseFails("1 2"));
    CHECK(ParseFails("{} x"));
    CHECK(ParseFails("// c\n1"));
    CHECK(ParseFails("01"));
    CHECK(ParseFails("-"));
    CHECK(ParseFails("1."));
    CHECK(ParseFails(".5"));
    CHECK(ParseFails("1e"));
    CHECK(ParseFails("+1"));
    CHECK(ParseFails("1e999"));
    CHECK(ParseFails("{\"a\":1,\"a\":2}"));

    CHECK(ParseFails("[1,\n  x]", 2, 3));
    CHECK(ParseFails("{\"a\":1,\"a\":2}", 1, 8));
}

std::string Nested(int depth, char open, char close)
{
    return std::string(static_cast<std::size_t>(depth), open) + std::string(static_cast<std::size_t>(depth), close);
}

void TestDepthLimit()
{
    JsonValue v;
    JsonError e;
    CHECK(Parse(Nested(kJsonMaxDepth, '[', ']'), v, e));
    CHECK(ParseFails(Nested(kJsonMaxDepth + 1, '[', ']')));
    CHECK(ParseFails(Nested(100000, '[', ']')));
    CHECK(ParseFails(std::string(100000, '[')));
}

void TestSizeLimit()
{
    JsonValue v;
    JsonError e;
    const std::string big(kJsonMaxTextSize + 1, ' ');
    CHECK(!ParseJson(big, v, e));
}

void TestEnemyLoader()
{
    std::map<int, EnemyData> d;
    JsonError e;

    CHECK(LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":80,\"radius\":16,\"hp\":3},"
        "{\"id\":2,\"name\":\"b\",\"speed\":1.5,\"radius\":4,\"hp\":1}]}",
        d, e));
    CHECK(d.size() == 2);
    CHECK(d.at(1).name == "a" && d.at(1).speed == 80.0f && d.at(1).radius == 16.0f && d.at(1).hp == 3);
    CHECK(d.at(2).speed == 1.5f && d.at(2).hp == 1);
    CHECK(d.begin()->first == 1);

    CHECK(LoadTableFromText<EnemyDataLoader>("{\"enemies\":[]}", d, e) && d.empty());

    CHECK(!LoadTableFromText<EnemyDataLoader>("[]", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>("{}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>("{\"enemies\":{}}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>("{\"enemies\":[1]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"name\":\"a\",\"speed\":80,\"radius\":16,\"hp\":1}]}", d, e));
    CHECK(e.message.find("id") != std::string::npos);
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1.5,\"name\":\"a\",\"speed\":80,\"radius\":16,\"hp\":1}]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":99999999999,\"name\":\"a\",\"speed\":80,\"radius\":16,\"hp\":1}]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":5,\"speed\":80,\"radius\":16,\"hp\":1}]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":\"fast\",\"radius\":16,\"hp\":1}]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":-1,\"radius\":16,\"hp\":1}]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":80,\"radius\":0,\"hp\":1}]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":80,\"radius\":16}]}", d, e));
    CHECK(e.message.find("hp") != std::string::npos);
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":80,\"radius\":16,\"hp\":0}]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":80,\"radius\":16,\"hp\":1.5}]}", d, e));
    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":80,\"radius\":16,\"hp\":1},"
        "{\"id\":1,\"name\":\"b\",\"speed\":80,\"radius\":16,\"hp\":1}]}", d, e));
    CHECK(e.message.find("중복") != std::string::npos);

    CHECK(!LoadTableFromText<EnemyDataLoader>(
        "{\"enemies\":[\n{\"id\":1,\"name\":\"a\",\"speed\":-1,\"radius\":16,\"hp\":1}]}", d, e));
    CHECK(e.line == 2);
}

void TestWaveLoader()
{
    std::map<int, WaveData> d;
    JsonError e;

    CHECK(LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":20,\"mode\":\"maintain\"}]}", d, e));
    CHECK(d.size() == 1 && d.at(1).count == 20 && d.at(1).archetype == 1 && d.at(1).mode == WaveMode::Maintain);

    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":20,\"mode\":\"burst\"}]}", d, e));
    CHECK(e.message.find("burst") != std::string::npos);
    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":-1,\"mode\":\"maintain\"}]}", d, e));
    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"mode\":\"maintain\"}]}", d, e));

    CHECK(LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":2,\"archetype\":3,\"count\":4,\"mode\":\"timed\","
        "\"start\":5,\"interval\":10.5,\"repeat\":3}]}", d, e));
    CHECK(d.at(2).mode == WaveMode::Timed && d.at(2).count == 4 && d.at(2).start == 5.0f &&
          d.at(2).interval == 10.5f && d.at(2).repeat == 3);
    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":4,\"mode\":\"timed\",\"interval\":1,\"repeat\":1}]}", d, e));
    CHECK(e.message.find("start") != std::string::npos);
    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":4,\"mode\":\"timed\",\"start\":0,\"repeat\":1}]}", d, e));
    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":4,\"mode\":\"timed\",\"start\":0,\"interval\":1}]}", d, e));
    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":4,\"mode\":\"timed\",\"start\":-1,\"interval\":1,\"repeat\":1}]}", d, e));
    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":4,\"mode\":\"timed\",\"start\":0,\"interval\":0,\"repeat\":1}]}", d, e));
    CHECK(!LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":4,\"mode\":\"timed\",\"start\":0,\"interval\":1,\"repeat\":0}]}", d, e));

    CHECK(LoadTableFromText<WaveDataLoader>(
        "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":4,\"mode\":\"maintain\",\"start\":9}]}", d, e));
    CHECK(d.at(1).start == 0.0f);
}

void TestDataManager()
{

    std::string err;

    DataManager legacy;
    CHECK(legacy.Load(std::string(GAME_DATA_DIR) + "/legacy_v1", err));
    if (!err.empty()) std::printf("  %s\n", err.c_str());
    const EnemyData* grunt = legacy.FindEnemy(1);
    CHECK(grunt != nullptr);
    if (grunt) {
        CHECK(grunt->speed == 80.0f);
        CHECK(grunt->radius == 16.0f);
        CHECK(grunt->hp == 1);
    }
    CHECK(legacy.Enemies().size() == 1);
    CHECK(legacy.Waves().size() == 1);
    CHECK(legacy.Waves().begin()->second.count == 20);
    CHECK(legacy.Waves().begin()->second.mode == WaveMode::Maintain);
    CHECK(legacy.DataHash() != 0);

    DataManager cur;
    CHECK(cur.Load(GAME_DATA_DIR, err));
    if (!err.empty()) std::printf("  %s\n", err.c_str());
    CHECK(cur.Enemies().size() == 3);
    CHECK(cur.FindEnemy(1) && cur.FindEnemy(1)->hp == 1);
    CHECK(cur.FindEnemy(2) && cur.FindEnemy(2)->radius == 8.0f);
    CHECK(cur.FindEnemy(3) && cur.FindEnemy(3)->radius == 32.0f && cur.FindEnemy(3)->hp == 3);
    CHECK(cur.Waves().size() == 3);
    int total = 0;
    for (const auto& w : cur.Waves()) {
        CHECK(w.second.mode == WaveMode::Maintain);
        total += w.second.count;
    }
    CHECK(total == 20);
    CHECK(cur.DataHash() != legacy.DataHash());

    DataManager demo;
    CHECK(demo.Load(std::string(GAME_DATA_DIR) + "/demo_timed", err));
    int timedCount = 0;
    for (const auto& w : demo.Waves()) timedCount += (w.second.mode == WaveMode::Timed) ? 1 : 0;
    CHECK(timedCount == 2);

    DataManager missing;
    CHECK(!missing.Load(std::string(GAME_DATA_DIR) + "/no_such_dir", err));
    CHECK(!err.empty());
}

void TestDataManagerValidation()
{

    const std::string enemies =
        "{\"enemies\":[{\"id\":1,\"name\":\"a\",\"speed\":80,\"radius\":16,\"hp\":1}]}";
    std::string err;

    DataManager ok;
    CHECK(ok.LoadFromText(enemies, "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":5,\"mode\":\"maintain\"}]}", err));

    DataManager noWaves;
    CHECK(!noWaves.LoadFromText(enemies, "{\"waves\":[]}", err));
    CHECK(err.find("웨이브가 하나도 없음") != std::string::npos);

    DataManager badRef;
    CHECK(!badRef.LoadFromText(enemies, "{\"waves\":[{\"id\":1,\"archetype\":9,\"count\":5,\"mode\":\"maintain\"}]}", err));
    CHECK(err.find("archetype") != std::string::npos);

    DataManager syntax;
    CHECK(!syntax.LoadFromText(enemies, "{\"waves\":[,]}", err, "e.json", "w.json"));
    CHECK(err.rfind("w.json:1:", 0) == 0);

    DataManager a, b, c;
    const std::string waves = "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":5,\"mode\":\"maintain\"}]}";
    CHECK(a.LoadFromText(enemies, waves, err) && b.LoadFromText(enemies, waves, err));
    CHECK(a.DataHash() == b.DataHash());
    CHECK(c.LoadFromText(enemies, "{\"waves\":[{\"id\":1,\"archetype\":1,\"count\":6,\"mode\":\"maintain\"}]}", err));
    CHECK(a.DataHash() != c.DataHash());
}

}

int main()
{
    TestParseValid();
    TestParseInvalid();
    TestDepthLimit();
    TestSizeLimit();
    TestEnemyLoader();
    TestWaveLoader();
    TestDataManager();
    TestDataManagerValidation();

    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
