#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain::decoration {

/// parity: worldgen iterates block positions out of a `Sets.newHashSet()`.
template <typename Pos>
class JavaHashSetOrder {
public:
    /// parity: `Vec3i.hashCode()`, folded the way `HashMap.hash()` folds it.
    [[nodiscard]] static std::uint32_t hashOf(const Pos& position) noexcept {
        const auto y = static_cast<std::uint32_t>(position.y);
        const auto z = static_cast<std::uint32_t>(position.z);
        const auto x = static_cast<std::uint32_t>(position.x);
        const std::uint32_t hash = ((y + z * 31U) * 31U) + x;
        return hash ^ (hash >> 16U);
    }

    bool add(const Pos& position) {
        std::vector<Pos>& bucket = buckets_[bucketOf(position)];
        if (std::find_if(bucket.begin(), bucket.end(), [&position](const Pos& held) { return held == position; }) !=
            bucket.end()) {
            return false;
        }
        bucket.push_back(position);
        ++size_;
        if (size_ > buckets_.size() * 3 / 4) {
            grow();
        }
        return true;
    }

    bool remove(const Pos& position) {
        std::vector<Pos>& bucket = buckets_[bucketOf(position)];
        const auto found =
            std::find_if(bucket.begin(), bucket.end(), [&position](const Pos& held) { return held == position; });
        if (found == bucket.end()) {
            return false;
        }
        bucket.erase(found);
        --size_;
        return true;
    }

    [[nodiscard]] bool contains(const Pos& position) const {
        const std::vector<Pos>& bucket = buckets_[bucketOf(position)];
        return std::find_if(bucket.begin(), bucket.end(), [&position](const Pos& held) { return held == position; }) !=
               bucket.end();
    }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] Pos first() const {
        for (const std::vector<Pos>& bucket : buckets_) {
            if (!bucket.empty()) {
                return bucket.front();
            }
        }
        return Pos{};
    }

    [[nodiscard]] Pos takeFirst() {
        const Pos position = first();
        static_cast<void>(remove(position));
        return position;
    }

    [[nodiscard]] std::vector<Pos> inOrder() const {
        std::vector<Pos> out;
        out.reserve(size_);
        for (const std::vector<Pos>& bucket : buckets_) {
            out.insert(out.end(), bucket.begin(), bucket.end());
        }
        return out;
    }

private:
    [[nodiscard]] std::size_t bucketOf(const Pos& position) const noexcept {
        return hashOf(position) & static_cast<std::uint32_t>(buckets_.size() - 1);
    }

    void grow() {
        const std::size_t oldCount = buckets_.size();
        std::vector<std::vector<Pos>> wider(oldCount * 2);
        for (std::size_t index = 0; index < oldCount; ++index) {
            for (const Pos& position : buckets_[index]) {
                const bool high = (hashOf(position) & static_cast<std::uint32_t>(oldCount)) != 0;
                wider[high ? index + oldCount : index].push_back(position);
            }
        }
        buckets_ = std::move(wider);
    }

    std::vector<std::vector<Pos>> buckets_{16};
    std::size_t size_ = 0;
};

template <typename Pos>
[[nodiscard]] std::vector<Pos> javaHashSetOrder(const std::vector<Pos>& positions) {
    if (positions.empty()) {
        return {};
    }
    std::size_t capacity = 16;
    while (positions.size() > capacity * 3 / 4) {
        capacity *= 2;
    }
    std::vector<Pos> ordered;
    ordered.reserve(positions.size());
    for (std::size_t bucket = 0; bucket < capacity; ++bucket) {
        for (const Pos& position : positions) {
            if ((JavaHashSetOrder<Pos>::hashOf(position) & static_cast<std::uint32_t>(capacity - 1)) == bucket) {
                ordered.push_back(position);
            }
        }
    }
    return ordered;
}

} // namespace basalt::server::world::terrain::decoration
