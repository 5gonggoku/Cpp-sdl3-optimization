#pragma once
#include "data/DataTypes.h"
#include "gameplay/GameObject.h"

class Enemy : public GameObject {
public:

    Enemy(Vector2 pos, const EnemyData* data, int waveIndex)
        : data_(data), hp_(data->hp), waveIndex_(waveIndex) { position = pos; }

    void SetTarget(Vector2 target) { target_ = target; }
    void Update(float dt) override;

    bool TakeDamage(int damage)
    {
        hp_ -= damage;
        if (hp_ <= 0) { alive = false; return true; }
        return false;
    }

    float Radius() const { return data_->radius; }
    int Hp() const { return hp_; }
    int WaveIndex() const { return waveIndex_; }
    int ArchetypeId() const { return data_->id; }

private:
    const EnemyData* data_;
    Vector2 target_;
    int hp_;
    int waveIndex_;
};
