#include <basalt/server/world/terrain/decoration/tree/JavaHashSetOrder.hpp>

#include <algorithm>

namespace basalt::server::world::terrain::decoration::tree {
namespace {

/// parity: `HashMap.hash()`, which folds the high half in before the table index is taken.
[[nodiscard]] std::uint32_t spread(std::int32_t hash) noexcept {
    const auto bits = static_cast<std::uint32_t>(hash);
    return bits ^ (bits >> 16U);
}

} // namespace

std::int32_t JavaHashSetOrder::hashOf(const BlockPos& position) noexcept {
    const auto y = static_cast<std::uint32_t>(position.y);
    const auto z = static_cast<std::uint32_t>(position.z);
    const auto x = static_cast<std::uint32_t>(position.x);
    return static_cast<std::int32_t>(((y + z * 31U) * 31U) + x);
}

std::size_t JavaHashSetOrder::bucketOf(std::int32_t hash) const noexcept {
    return spread(hash) & static_cast<std::uint32_t>(buckets_.size() - 1);
}

bool JavaHashSetOrder::contains(const BlockPos& position) const {
    const std::vector<BlockPos>& bucket = buckets_[bucketOf(hashOf(position))];
    return std::find(bucket.begin(), bucket.end(), position) != bucket.end();
}

bool JavaHashSetOrder::add(const BlockPos& position) {
    std::vector<BlockPos>& bucket = buckets_[bucketOf(hashOf(position))];
    if (std::find(bucket.begin(), bucket.end(), position) != bucket.end()) {
        return false;
    }
    bucket.push_back(position);
    ++size_;
    // Java grows once the count passes three quarters of the table, after the entry went in.
    if (size_ > buckets_.size() * 3 / 4) {
        resize();
    }
    return true;
}

bool JavaHashSetOrder::remove(const BlockPos& position) {
    std::vector<BlockPos>& bucket = buckets_[bucketOf(hashOf(position))];
    const auto found = std::find(bucket.begin(), bucket.end(), position);
    if (found == bucket.end()) {
        return false;
    }
    bucket.erase(found);
    --size_;
    return true;
}

void JavaHashSetOrder::resize() {
    // Each bucket splits in two by the bit the wider table adds, and both halves keep their order.
    const std::size_t oldCount = buckets_.size();
    std::vector<std::vector<BlockPos>> wider(oldCount * 2);
    for (std::size_t index = 0; index < oldCount; ++index) {
        for (const BlockPos& position : buckets_[index]) {
            const bool high = (spread(hashOf(position)) & static_cast<std::uint32_t>(oldCount)) != 0;
            wider[high ? index + oldCount : index].push_back(position);
        }
    }
    buckets_ = std::move(wider);
}

BlockPos JavaHashSetOrder::first() const {
    for (const std::vector<BlockPos>& bucket : buckets_) {
        if (!bucket.empty()) {
            return bucket.front();
        }
    }
    return BlockPos{};
}

std::vector<BlockPos> JavaHashSetOrder::inOrder() const {
    std::vector<BlockPos> out;
    out.reserve(size_);
    for (const std::vector<BlockPos>& bucket : buckets_) {
        out.insert(out.end(), bucket.begin(), bucket.end());
    }
    return out;
}

} // namespace basalt::server::world::terrain::decoration::tree
