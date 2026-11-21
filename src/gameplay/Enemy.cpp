#include "gameplay/Enemy.h"

void Enemy::Update(float dt)
{
    const Vector2 toTarget = target_ - position;
    if (toTarget.LengthSq() > 0.0f) {
        position += toTarget.Normalized() * data_->speed * dt;
    }
}
