#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "math/Vector2.h"

class SpatialGrid {
public:

    void Init(float width, float height, float cellSize)
    {
        cellSize_ = cellSize;
        invCell_ = 1.0f / cellSize;
        cols_ = static_cast<int>(width / cellSize) + 1;
        rows_ = static_cast<int>(height / cellSize) + 1;
        const std::size_t cells = static_cast<std::size_t>(cols_) * static_cast<std::size_t>(rows_);
        cellStart_.assign(cells + 1, 0);
        cursor_.assign(cells, 0);
        items_.clear();
        cellOf_.clear();
    }

    template <typename GetPos>
    void Build(std::size_t count, GetPos&& getPos)
    {
        const std::size_t cells = cursor_.size();
        std::fill(cellStart_.begin(), cellStart_.end(), 0);
        cellOf_.resize(count);
        items_.resize(count);

        for (std::size_t i = 0; i < count; ++i) {
            const Vector2 p = getPos(i);
            const int cell = CellY(p.y) * cols_ + CellX(p.x);
            cellOf_[i] = cell;
            ++cellStart_[static_cast<std::size_t>(cell) + 1];
        }
        for (std::size_t c = 0; c < cells; ++c) cellStart_[c + 1] += cellStart_[c];
        std::copy(cellStart_.begin(), cellStart_.begin() + static_cast<std::ptrdiff_t>(cells), cursor_.begin());
        for (std::size_t i = 0; i < count; ++i) {
            items_[static_cast<std::size_t>(cursor_[static_cast<std::size_t>(cellOf_[i])]++)] = static_cast<int>(i);
        }
    }

    template <typename Pred>
    int FindFirst(Vector2 center, Pred&& pred) const
    {
        const int cx = CellX(center.x);
        const int cy = CellY(center.y);
        const int y0 = std::max(cy - 1, 0), y1 = std::min(cy + 1, rows_ - 1);
        const int x0 = std::max(cx - 1, 0), x1 = std::min(cx + 1, cols_ - 1);

        int best = -1;
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const std::size_t cell = static_cast<std::size_t>(y * cols_ + x);
                for (int k = cellStart_[cell]; k < cellStart_[cell + 1]; ++k) {
                    const int index = items_[static_cast<std::size_t>(k)];
                    if (best >= 0 && index >= best) break;
                    if (pred(index)) { best = index; break; }
                }
            }
        }
        return best;
    }

    float CellSize() const { return cellSize_; }
    int Cols() const { return cols_; }
    int Rows() const { return rows_; }

private:

    int CellX(float x) const { return ToCell(x, cols_); }
    int CellY(float y) const { return ToCell(y, rows_); }
    int ToCell(float v, int count) const
    {
        return static_cast<int>(std::clamp(v * invCell_, 0.0f, static_cast<float>(count - 1)));
    }

    float cellSize_ = 1.0f;
    float invCell_ = 1.0f;
    int cols_ = 1;
    int rows_ = 1;
    std::vector<int> cellStart_;
    std::vector<int> cursor_;
    std::vector<int> cellOf_;
    std::vector<int> items_;
};
