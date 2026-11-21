#pragma once
#include "math/Vector2.h"

class GameObject {
public:
    virtual ~GameObject() = default;
    virtual void Update(float dt) = 0;

    Vector2 position;
    bool alive = true;
};
