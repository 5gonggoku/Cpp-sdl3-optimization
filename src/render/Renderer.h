#pragma once

struct SDL_Renderer;
class World;

class Renderer {
public:
    explicit Renderer(SDL_Renderer* renderer) : renderer_(renderer) {}

    void Draw(const World& world) const;

private:
    SDL_Renderer* renderer_ = nullptr;
};
