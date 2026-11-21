#pragma once
#include <map>
#include <string>
#include <type_traits>
#include <vector>

#include "data/Json.h"

template <typename Key, typename Value>
class ILoader {
public:
    virtual ~ILoader() = default;

    virtual bool FromJson(const JsonValue& root, JsonError& err) = 0;

    virtual bool MakeDict(std::map<Key, Value>& out, JsonError& err) = 0;
};

template <typename Key, typename Value, typename GetKey>
bool MoveItemsToDict(std::vector<Value>& items, std::map<Key, Value>& out, GetKey getKey, JsonError& err)
{
    out.clear();
    for (Value& item : items) {
        const Key key = getKey(item);
        if (out.find(key) != out.end()) {
            err = JsonError{};
            err.message = "중복된 id: " + std::to_string(key);
            return false;
        }
        out.emplace(key, std::move(item));
    }
    items.clear();
    return true;
}

template <typename Loader, typename Key, typename Value>
bool LoadTableFromText(const std::string& text, std::map<Key, Value>& out, JsonError& err)
{
    static_assert(std::is_base_of<ILoader<Key, Value>, Loader>::value,
                  "Loader는 ILoader<Key, Value>를 상속해야 함");
    JsonValue root;
    if (!ParseJson(text, root, err)) return false;
    Loader loader;
    if (!loader.FromJson(root, err)) return false;
    return loader.MakeDict(out, err);
}

template <typename Loader, typename Key, typename Value>
bool LoadTable(const std::string& path, std::map<Key, Value>& out, std::string& errMsg,
               std::string* rawText = nullptr)
{
    std::string text;
    if (!ReadTextFile(path, text, errMsg)) return false;
    JsonError err;
    if (!LoadTableFromText<Loader>(text, out, err)) {
        errMsg = FormatError(path, err);
        return false;
    }
    if (rawText) *rawText = std::move(text);
    return true;
}
