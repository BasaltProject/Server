#pragma once

#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <span>
#include <vector>

namespace basalt::server::world {

/// parity: `PalettedContainer`.
class PalettedContainer {
public:
    explicit PalettedContainer(std::size_t size, std::int32_t initialValue = 0);

    /// From here on other threads may read it, so every access takes the lock; before this only its owner touches it.
    void share() noexcept { shared_.store(true, std::memory_order_release); }

    [[nodiscard]] std::int32_t get(std::size_t index) const;

    bool set(std::size_t index, std::int32_t value);

    [[nodiscard]] std::vector<std::int32_t> toVector() const;

    void copyInto(std::span<std::int32_t> dest) const;

    /// Writes `table[value]` for every value, or `fallback` for a value past the table.
    void copyMappedInto(std::span<std::uint8_t> dest, std::span<const std::int32_t> table, std::uint8_t fallback) const;

    void setAll(std::span<const std::int32_t> values);

    [[nodiscard]] std::size_t distinctValueCount() const;

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

    [[nodiscard]] std::size_t residentBytes() const;

    [[nodiscard]] int bitsPerValue() const;

private:
    std::size_t indexOfOrAdd(std::int32_t value);

    [[nodiscard]] std::int32_t at(std::size_t index) const;

    std::size_t size_;
    mutable std::shared_mutex lock_;
    std::atomic<bool> shared_{false};
    int bits_ = 0;
    std::vector<std::int32_t> palette_;
    std::size_t paletteSize_ = 0;
    std::vector<std::uint64_t> words_;
};

} // namespace basalt::server::world
