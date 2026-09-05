#pragma once

#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace basalt::server::world::terrain::decoration::tree {

/// parity: `TreeFeature` keeps its trunks, foliage and leaf buckets in `Sets.newHashSet()`, a plain
/// `java.util.HashSet`, and iterates them. That order is not insertion order, and it is observable:
/// the leaf pass writes a distance twice into the same leaf and the later write wins, so which one
/// comes second decides the number the block ends up with.
///
/// Modelled here as the map behind that set: buckets in table order, entries within a bucket in the
/// order they were appended, the table doubling at three quarters full and each bucket splitting in
/// two while keeping its relative order. A bucket that reaches eight entries becomes a tree in Java
/// and reorders; with block positions spread over a tree's bounding box that does not happen, and
/// the fallback here would be the plain list.
class JavaHashSetOrder {
public:
    /// Adds unless already present, answering whether it went in.
    bool add(const BlockPos& position);

    /// Removes it if present, answering whether it was there.
    bool remove(const BlockPos& position);

    [[nodiscard]] bool contains(const BlockPos& position) const;

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /// The first position the set's own iterator would answer.
    [[nodiscard]] BlockPos first() const;

    /// Every position in the set's iteration order.
    [[nodiscard]] std::vector<BlockPos> inOrder() const;

    /// parity: `Vec3i.hashCode()`, which is what `HashMap` spreads and buckets by.
    [[nodiscard]] static std::int32_t hashOf(const BlockPos& position) noexcept;

private:
    void resize();

    [[nodiscard]] std::size_t bucketOf(std::int32_t hash) const noexcept;

    std::vector<std::vector<BlockPos>> buckets_{16};
    std::size_t size_ = 0;
};

} // namespace basalt::server::world::terrain::decoration::tree
