#pragma once

#include <cstdint>
#include <shared_mutex>
#include <span>
#include <vector>

namespace basalt::server::world {

/// parity: `PalettedContainer`, as storage only. Holds `size` values while paying for the distinct
/// ones, which is what makes chunks affordable to keep in memory.
///
/// One writer, any number of readers, under a shared mutex: the width, the palette and the packed
/// words cannot be read separately, or a widening write hands a reader the new width with the old
/// array. `get` and `set` take the lock per value; the bulk paths take it once per section.
class PalettedContainer {
public:
    explicit PalettedContainer(std::size_t size, std::int32_t initialValue = 0);

    [[nodiscard]] std::int32_t get(std::size_t index) const;

    /// Whether the stored value actually changed, so callers can track modifications without
    /// re-reading.
    bool set(std::size_t index, std::int32_t value);

    /// Every value, expanded. Allocates: for the save and network paths, which need a flat view once.
    [[nodiscard]] std::vector<std::int32_t> toVector() const;

    /// Expands into `dest`, which must hold at least `size` values.
    ///
    /// Takes the snapshot once rather than once per value: a save expanding every section of every
    /// chunk would otherwise take it four thousand times a section.
    void copyInto(std::span<std::int32_t> dest) const;

    /// Replaces the whole contents, re-deriving the palette from `values`.
    void setAll(std::span<const std::int32_t> values);

    /// How many distinct values are stored, which is what decides the entry width.
    [[nodiscard]] std::size_t distinctValueCount() const;

    /// Whether any stored value satisfies `predicate`, answered from the palette alone.
    ///
    /// This is what makes a whole-world search affordable: a section keeps one entry per *distinct*
    /// value, so asking it is a handful of comparisons rather than four thousand, and a section that
    /// cannot hold what is being looked for is skipped without reading a single packed word.
    template <typename Predicate>
    [[nodiscard]] bool anyValue(Predicate&& predicate) const {
        const std::shared_lock<std::shared_mutex> guard{lock_};
        for (std::size_t i = 0; i < paletteSize_; ++i) {
            if (predicate(palette_[i])) {
                return true;
            }
        }
        return false;
    }

    /// Approximate bytes this container keeps resident: palette plus packed words, headers ignored.
    [[nodiscard]] std::size_t residentBytes() const;

    /// Bits each entry occupies; 0 while every entry is identical and no data array exists.
    [[nodiscard]] int bitsPerValue() const;

private:
    /// The palette slot for `value`, adding it and widening the entries if it is new. Called with
    /// the lock already held.
    std::size_t indexOfOrAdd(std::int32_t value);

    [[nodiscard]] std::int32_t at(std::size_t index) const;

    std::size_t size_;
    mutable std::shared_mutex lock_;
    /// 0 bits means every entry is `palette_[0]`, in which case `words_` stays empty and costs
    /// nothing at all.
    int bits_ = 0;
    std::vector<std::int32_t> palette_;
    std::size_t paletteSize_ = 0;
    std::vector<std::uint64_t> words_;
};

} // namespace basalt::server::world
