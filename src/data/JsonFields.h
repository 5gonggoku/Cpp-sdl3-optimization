#pragma once
#include "data/Json.h"

const JsonValue* RequireArrayField(const JsonValue& root, const char* key, JsonError& err);

bool ReadInt(const JsonValue& obj, const char* key, int& out, JsonError& err);
bool ReadFloat(const JsonValue& obj, const char* key, float& out, JsonError& err);
bool ReadString(const JsonValue& obj, const char* key, std::string& out, JsonError& err);
