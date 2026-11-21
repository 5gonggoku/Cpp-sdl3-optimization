#include "gameplay/Player.h"

void Player::Update(float dt, Vector2 moveDir)
{
    position += moveDir * kMoveSpeed * dt;
}
