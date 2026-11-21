#pragma once
#include "math/Vector2.h"

class Player {
public:
    void Update(float dt, Vector2 moveDir);

    Vector2 position;

    static constexpr float kMoveSpeed = 300.0f;
    static constexpr float kSize = 50.0f;
};
