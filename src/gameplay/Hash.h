#pragma once
#include <cstdint>
#include <cstring>

class Fnv1a {
public:
    void AddBytes(const void* data, std::size_t size)
    {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < size; ++i) {
            hash_ ^= p[i];
            hash_ *= kPrime;
        }
    }

    void AddFloat(float v)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        AddBytes(&bits, sizeof(bits));
    }

    void AddU64(std::uint64_t v) { AddBytes(&v, sizeof(v)); }

    std::uint64_t Value() const { return hash_; }

private:
    static constexpr std::uint64_t kOffset = 14695981039346656037ull;
    static constexpr std::uint64_t kPrime = 1099511628211ull;
    std::uint64_t hash_ = kOffset;
};
