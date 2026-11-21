#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#if defined(__SANITIZE_ADDRESS__) && !defined(POOL_NO_ASAN_POISON)
#include <sanitizer/asan_interface.h>
#define POOL_POISON_REGION(ptr, bytes) ASAN_POISON_MEMORY_REGION((ptr), (bytes))
#define POOL_UNPOISON_REGION(ptr, bytes) ASAN_UNPOISON_MEMORY_REGION((ptr), (bytes))
#else
#define POOL_POISON_REGION(ptr, bytes) ((void)0)
#define POOL_UNPOISON_REGION(ptr, bytes) ((void)0)
#endif

template <typename T, std::size_t BlockSize = 1024>
class Pool {
public:
    static_assert(BlockSize > 0, "BlockSize must be positive");

    Pool() = default;
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    ~Pool()
    {
        for (std::size_t i = 0; i < live_.size(); ++i) {
            if (live_[i]) SlotAt(i)->~T();
        }
        for (T* block : blocks_) {
            POOL_UNPOISON_REGION(block, sizeof(T) * BlockSize);
            alloc_.deallocate(block, BlockSize);
        }
    }

    template <typename... Args>
    T* Acquire(Args&&... args)
    {
        if (freeList_.empty()) AddBlock();
        const std::size_t index = freeList_.back();
        freeList_.pop_back();
        T* slot = SlotAt(index);
        POOL_UNPOISON_REGION(slot, sizeof(T));
        ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);
        live_[index] = 1;
        ++liveCount_;
        return slot;
    }

    bool Release(T* object)
    {
        for (std::size_t b = 0; b < blocks_.size(); ++b) {
            const T* base = blocks_[b];
            if (object >= base && object < base + BlockSize) {
                const std::size_t index = b * BlockSize + static_cast<std::size_t>(object - base);
                if (!live_[index]) return false;
                object->~T();
                POOL_POISON_REGION(object, sizeof(T));
                live_[index] = 0;
                --liveCount_;
                freeList_.push_back(index);
                return true;
            }
        }
        return false;
    }

    std::size_t LiveCount() const { return liveCount_; }
    std::size_t Capacity() const { return blocks_.size() * BlockSize; }
    std::size_t BlockCount() const { return blocks_.size(); }

private:
    T* SlotAt(std::size_t index) const { return blocks_[index / BlockSize] + (index % BlockSize); }

    void AddBlock()
    {
        const std::size_t firstNew = blocks_.size() * BlockSize;
        blocks_.reserve(blocks_.size() + 1);
        live_.reserve(firstNew + BlockSize);
        freeList_.reserve(firstNew + BlockSize);
        T* block = alloc_.allocate(BlockSize);
        POOL_POISON_REGION(block, sizeof(T) * BlockSize);
        blocks_.push_back(block);
        live_.resize(firstNew + BlockSize, 0);

        for (std::size_t i = BlockSize; i > 0; --i) freeList_.push_back(firstNew + i - 1);
    }

    std::allocator<T> alloc_;
    std::vector<T*> blocks_;
    std::vector<std::uint8_t> live_;
    std::vector<std::size_t> freeList_;
    std::size_t liveCount_ = 0;
};
