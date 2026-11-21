#pragma once
#include <vector>

inline std::vector<int> DistributeByWeight(int total, const std::vector<int>& weights)
{
    std::vector<int> shares(weights.size(), 0);
    long long sum = 0;
    for (const int w : weights) sum += w;
    if (sum <= 0 || total <= 0) return shares;

    long long assigned = 0;
    std::size_t largest = 0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        shares[i] = static_cast<int>(static_cast<long long>(total) * weights[i] / sum);
        assigned += shares[i];
        if (weights[i] > weights[largest]) largest = i;
    }
    shares[largest] += static_cast<int>(total - assigned);
    return shares;
}
