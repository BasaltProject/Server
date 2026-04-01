#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::entity {

/// The order fastutil's `Int2ObjectOpenHashMap` keeps its keys in, with nothing stored beside them.
///
/// parity: open addressing by `HashCommon.mix`, linear probing, backward-shift deletion, and an
/// iterator that walks the table from the top and remembers what a removal shifted past it. Zero is
/// the empty slot, as there; a key of zero is not supported.
class IntHashOrder {
public:
    IntHashOrder();

    void insert(std::int32_t key);

    void remove(std::int32_t key);

    [[nodiscard]] std::int32_t size() const noexcept { return size_; }

    /// The keys as `values()` or `keySet()` would walk them.
    [[nodiscard]] std::vector<std::int32_t> order() const;

    /// Walks the keys as an iterator does; `visit` answers true to remove the key it was given.
    void iterate(const std::function<bool(std::int32_t key)>& visit);

private:
    [[nodiscard]] static std::uint32_t mix(std::int32_t key);
    void shiftKeys(std::int32_t pos, std::vector<std::int32_t>* wrapped);
    void rehash(std::int32_t capacity);

    std::vector<std::int32_t> keys_;
    std::int32_t mask_ = 0;
    std::int32_t size_ = 0;
    std::int32_t maxFill_ = 0;
};

} // namespace basalt::server::runtime::entity
