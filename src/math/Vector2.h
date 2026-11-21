#pragma once
#include <cmath>

struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    Vector2() = default;
    Vector2(float px, float py) : x(px), y(py) {}

    Vector2 operator+(const Vector2& o) const { return { x + o.x, y + o.y }; }
    Vector2 operator-(const Vector2& o) const { return { x - o.x, y - o.y }; }
    Vector2 operator*(float s) const { return { x * s, y * s }; }
    Vector2& operator+=(const Vector2& o) { x += o.x; y += o.y; return *this; }

    float Dot(const Vector2& o) const { return x * o.x + y * o.y; }
    float LengthSq() const { return Dot(*this); }
    float Length() const { return std::sqrt(LengthSq()); }
    Vector2 Normalized() const {
        const float len = Length();
        return len > 0.0f ? Vector2{ x / len, y / len } : Vector2{};
    }
};
