#pragma once
#include "gameplay/GameObject.h"

class Bullet : public GameObject {
public:
    Bullet(Vector2 pos, Vector2 velocity, float lifetime)
        : velocity_(velocity), lifetime_(lifetime) { position = pos; }

    void Update(float dt) override;

    static constexpr float kRadius = 6.0f;

private:
    Vector2 velocity_;
    float lifetime_;
    float age_ = 0.0f;
};
