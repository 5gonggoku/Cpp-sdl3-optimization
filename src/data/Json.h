#pragma once
#include <string>
#include <vector>

struct JsonError {
    int line = 0;
    int column = 0;
    std::string message;
};

std::string FormatError(const std::string& path, const JsonError& err);

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::vector<std::string> keys;
    std::vector<JsonValue> values;
    int line = 0;
    int column = 0;

    const JsonValue* Find(const std::string& key) const;
};

constexpr int kJsonMaxDepth = 64;
constexpr std::size_t kJsonMaxTextSize = 1 << 20;

bool ParseJson(const std::string& text, JsonValue& out, JsonError& err);

bool ReadTextFile(const std::string& path, std::string& out, std::string& errMsg);
