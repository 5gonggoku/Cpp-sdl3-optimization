#include "data/JsonFields.h"

#include <cmath>
#include <limits>

namespace {

bool Fail(const JsonValue& at, JsonError& err, const std::string& message)
{
    err.line = at.line;
    err.column = at.column;
    err.message = message;
    return false;
}

const JsonValue* RequireField(const JsonValue& obj, const char* key, JsonError& err)
{
    if (obj.type != JsonType::Object) {
        Fail(obj, err, "객체가 필요함");
        return nullptr;
    }
    const JsonValue* v = obj.Find(key);
    if (!v) {
        Fail(obj, err, std::string("필수 필드 없음: ") + key);
        return nullptr;
    }
    return v;
}

}

const JsonValue* RequireArrayField(const JsonValue& root, const char* key, JsonError& err)
{
    const JsonValue* v = RequireField(root, key, err);
    if (!v) return nullptr;
    if (v->type != JsonType::Array) {
        Fail(*v, err, std::string("배열이어야 함: ") + key);
        return nullptr;
    }
    return v;
}

bool ReadInt(const JsonValue& obj, const char* key, int& out, JsonError& err)
{
    const JsonValue* v = RequireField(obj, key, err);
    if (!v) return false;
    if (v->type != JsonType::Number) return Fail(*v, err, std::string("숫자여야 함: ") + key);
    const double d = v->number;

    if (d != std::floor(d) || d < static_cast<double>(std::numeric_limits<int>::min()) ||
        d > static_cast<double>(std::numeric_limits<int>::max())) {
        return Fail(*v, err, std::string("정수여야 함: ") + key);
    }
    out = static_cast<int>(d);
    return true;
}

bool ReadFloat(const JsonValue& obj, const char* key, float& out, JsonError& err)
{
    const JsonValue* v = RequireField(obj, key, err);
    if (!v) return false;
    if (v->type != JsonType::Number) return Fail(*v, err, std::string("숫자여야 함: ") + key);
    if (std::fabs(v->number) > static_cast<double>(std::numeric_limits<float>::max())) {
        return Fail(*v, err, std::string("float 범위를 벗어남: ") + key);
    }
    out = static_cast<float>(v->number);
    return true;
}

bool ReadString(const JsonValue& obj, const char* key, std::string& out, JsonError& err)
{
    const JsonValue* v = RequireField(obj, key, err);
    if (!v) return false;
    if (v->type != JsonType::String) return Fail(*v, err, std::string("문자열이어야 함: ") + key);
    out = v->string;
    return true;
}
