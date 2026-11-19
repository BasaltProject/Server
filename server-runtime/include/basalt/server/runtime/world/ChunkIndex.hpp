#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace basalt::server::world {
class Chunk;
}

namespace basalt::server::runtime::world {

class ChunkIndex {
public:
    [[nodiscard]] server::world::Chunk* find(std::int64_t key) const {
        if (slots_.empty()) {
            return nullptr;
        }
        const std::size_t mask = slots_.size() - 1;
        for (std::size_t at = hashOf(key) & mask; slots_[at].chunk != nullptr; at = (at + 1) & mask) {
            if (slots_[at].key == key) {
                return slots_[at].chunk;
            }
        }
        return nullptr;
    }

    void emplace(std::int64_t key, server::world::Chunk* chunk) {
        if ((used_ + 1) * 2 > slots_.size()) {
            grow();
        }
        place(key, chunk);
    }

    void erase(std::int64_t key) {
        if (slots_.empty()) {
            return;
        }
        const std::size_t mask = slots_.size() - 1;
        std::size_t at = hashOf(key) & mask;
        while (slots_[at].chunk != nullptr && slots_[at].key != key) {
            at = (at + 1) & mask;
        }
        if (slots_[at].chunk == nullptr) {
            return;
        }
        slots_[at].chunk = nullptr;
        --used_;
        for (std::size_t next = (at + 1) & mask; slots_[next].chunk != nullptr; next = (next + 1) & mask) {
            const Slot moved = slots_[next];
            slots_[next].chunk = nullptr;
            --used_;
            place(moved.key, moved.chunk);
        }
    }

private:
    struct Slot {
        std::int64_t key = 0;
        server::world::Chunk* chunk = nullptr;
    };

    [[nodiscard]] static std::size_t hashOf(std::int64_t key) {
        return static_cast<std::size_t>((static_cast<std::uint64_t>(key) * 0x9E3779B97F4A7C15ULL) >> 32);
    }

    void place(std::int64_t key, server::world::Chunk* chunk) {
        const std::size_t mask = slots_.size() - 1;
        std::size_t at = hashOf(key) & mask;
        while (slots_[at].chunk != nullptr) {
            if (slots_[at].key == key) {
                return;
            }
            at = (at + 1) & mask;
        }
        slots_[at] = Slot{.key = key, .chunk = chunk};
        ++used_;
    }

    void grow() {
        std::vector<Slot> old(slots_.empty() ? 64 : slots_.size() * 2);
        old.swap(slots_);
        used_ = 0;
        for (const Slot& slot : old) {
            if (slot.chunk != nullptr) {
                place(slot.key, slot.chunk);
            }
        }
    }

    std::vector<Slot> slots_;
    std::size_t used_ = 0;
};

} // namespace basalt::server::runtime::world
