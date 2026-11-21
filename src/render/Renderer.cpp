#include "render/Renderer.h"

#include <SDL3/SDL.h>

#include "gameplay/World.h"

void Renderer::Draw(const World& world) const
{
    SDL_SetRenderDrawColor(renderer_, 20, 24, 32, 255);
    SDL_RenderClear(renderer_);

    SDL_SetRenderDrawColor(renderer_, 220, 60, 60, 255);
    for (const auto& enemy : world.GetEnemies()) {
        const Vector2 pos = enemy->position;
        const float r = enemy->Radius();
        const SDL_FRect rect{ pos.x - r, pos.y - r, r * 2.0f, r * 2.0f };
        SDL_RenderFillRect(renderer_, &rect);
    }

    SDL_SetRenderDrawColor(renderer_, 230, 210, 60, 255);
    for (const auto& bullet : world.GetBullets()) {
        const Vector2 pos = bullet->position;
        const float r = Bullet::kRadius;
        const SDL_FRect rect{ pos.x - r, pos.y - r, r * 2.0f, r * 2.0f };
        SDL_RenderFillRect(renderer_, &rect);
    }

    SDL_SetRenderDrawColor(renderer_, 80, 200, 120, 255);
    const Vector2 pos = world.GetPlayer().position;
    const SDL_FRect rect{ pos.x, pos.y, Player::kSize, Player::kSize };
    SDL_RenderFillRect(renderer_, &rect);
}
