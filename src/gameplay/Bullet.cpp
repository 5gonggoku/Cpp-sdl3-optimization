#include "gameplay/Bullet.h"

void Bullet::Update(float dt)
{
    position += velocity_ * dt;
    age_ += dt;
    if (age_ >= lifetime_) {
        alive = false;
    }
}
