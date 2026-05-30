#include <basalt/server/world/PalettedContainer.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <stdexcept>

namespace basalt::server::world {
namespace {

constexpr std::size_t kInitialPaletteCapacity = 4;

constexpr int kMinBitsPerEntry = 4;

[[nodiscard]] int bitsFor(std::size_t entries) {
    int bits = 0;
    while ((std::size_t{1} << bits) < entries) {
        bits++;
    }
    return bits;
}

[[nodiscard]] std::size_t readRaw(std::span<const std::uint64_t> words, int bits, std::size_t index) {
    const std::size_t bitIndex = index * static_cast<std::size_t>(bits);
    const std::size_t wordIndex = bitIndex >> 6;
    const std::size_t bitOffset = bitIndex & 63;
    const std::uint64_t mask = (std::uint64_t{1} << bits) - 1;
    const std::size_t available = 64 - bitOffset;
    if (available >= static_cast<std::size_t>(bits)) {
        return static_cast<std::size_t>((words[wordIndex] >> bitOffset) & mask);
    }
    // An entry straddling two words: the low bits finish in the next one.
    return static_cast<std::size_t>(((words[wordIndex] >> bitOffset) | (words[wordIndex + 1] << available)) & mask);
}

void writeInto(std::span<std::uint64_t> words, int bits, std::size_t index, std::size_t paletteIndex) {
    const std::size_t bitIndex = index * static_cast<std::size_t>(bits);
    const std::size_t wordIndex = bitIndex >> 6;
    const std::size_t bitOffset = bitIndex & 63;
    const std::uint64_t mask = (std::uint64_t{1} << bits) - 1;
    const std::uint64_t value = static_cast<std::uint64_t>(paletteIndex) & mask;
    const std::size_t available = 64 - bitOffset;
    words[wordIndex] = (words[wordIndex] & ~(mask << bitOffset)) | (value << bitOffset);
    if (available < static_cast<std::size_t>(bits)) {
        words[wordIndex + 1] = (words[wordIndex + 1] & ~(mask >> available)) | (value >> available);
    }
}

} // namespace

PalettedContainer::PalettedContainer(std::size_t size, std::int32_t initialValue)
    : size_(size), palette_(kInitialPaletteCapacity, 0), paletteSize_(1) {
    palette_[0] = initialValue;
}

std::int32_t PalettedContainer::at(std::size_t index) const {
    if (words_.empty()) {
        return palette_[0];
    }
    return palette_[readRaw(words_, bits_, index)];
}

std::int32_t PalettedContainer::get(std::size_t index) const {
    if (!shared_.load(std::memory_order_acquire)) {
        return at(index);
    }
    const std::shared_lock<std::shared_mutex> guard{lock_};
    return at(index);
}

bool PalettedContainer::set(std::size_t index, std::int32_t value) {
    std::unique_lock<std::shared_mutex> guard{lock_, std::defer_lock};
    if (shared_.load(std::memory_order_acquire)) {
        guard.lock();
    }
    if (words_.empty() && palette_[0] == value) {
        return false;
    }
    if (at(index) == value) {
        return false;
    }
    const std::size_t slot = indexOfOrAdd(value);
    if (!words_.empty()) {
        writeInto(words_, bits_, index, slot);
    }
    return true;
}

std::vector<std::int32_t> PalettedContainer::toVector() const {
    std::vector<std::int32_t> values(size_);
    copyInto(values);
    return values;
}

void PalettedContainer::copyInto(std::span<std::int32_t> dest) const {
    if (dest.size() < size_) {
        throw std::invalid_argument{"paletted container needs room for every value"};
    }
    const std::shared_lock<std::shared_mutex> guard{lock_};
    if (words_.empty()) {
        std::fill_n(dest.begin(), size_, palette_[0]);
        return;
    }
    const auto width = static_cast<unsigned>(bits_);
    const std::uint64_t mask = (std::uint64_t{1} << width) - 1;
    unsigned __int128 buffer = 0;
    unsigned buffered = 0;
    std::size_t nextWord = 0;
    for (std::size_t i = 0; i < size_; ++i) {
        if (buffered < width) {
            buffer |= static_cast<unsigned __int128>(words_[nextWord++]) << buffered;
            buffered += 64;
        }
        dest[i] = palette_[static_cast<std::size_t>(static_cast<std::uint64_t>(buffer) & mask)];
        buffer >>= width;
        buffered -= width;
    }
}

void PalettedContainer::copyMappedInto(std::span<std::uint8_t> dest, std::span<const std::int32_t> table,
                                       std::uint8_t fallback) const {
    if (dest.size() < size_) {
        throw std::invalid_argument{"paletted container needs room for every value"};
    }
    const std::shared_lock<std::shared_mutex> guard{lock_};
    std::array<std::uint8_t, 4096> mapped{};
    std::vector<std::uint8_t> overflow;
    std::uint8_t* lookup = mapped.data();
    if (paletteSize_ > mapped.size()) {
        overflow.resize(paletteSize_);
        lookup = overflow.data();
    }
    for (std::size_t i = 0; i < paletteSize_; ++i) {
        const std::int32_t value = palette_[i];
        lookup[i] = value >= 0 && static_cast<std::size_t>(value) < table.size()
                        ? static_cast<std::uint8_t>(table[static_cast<std::size_t>(value)])
                        : fallback;
    }
    if (words_.empty()) {
        std::fill_n(dest.begin(), size_, lookup[0]);
        return;
    }
    const auto width = static_cast<unsigned>(bits_);
    const std::uint64_t mask = (std::uint64_t{1} << width) - 1;
    unsigned __int128 buffer = 0;
    unsigned buffered = 0;
    std::size_t nextWord = 0;
    for (std::size_t i = 0; i < size_; ++i) {
        if (buffered < width) {
            buffer |= static_cast<unsigned __int128>(words_[nextWord++]) << buffered;
            buffered += 64;
        }
        dest[i] = lookup[static_cast<std::uint64_t>(buffer) & mask];
        buffer >>= width;
        buffered -= width;
    }
}

void PalettedContainer::setAll(std::span<const std::int32_t> values) {
    if (values.size() != size_) {
        throw std::invalid_argument{"paletted container was given the wrong number of values"};
    }
    const std::unique_lock<std::shared_mutex> guard{lock_};
    bits_ = 0;
    words_.clear();
    palette_.assign(kInitialPaletteCapacity, 0);
    palette_[0] = values[0];
    paletteSize_ = 1;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (at(i) == values[i]) {
            continue;
        }
        const std::size_t slot = indexOfOrAdd(values[i]);
        if (!words_.empty()) {
            writeInto(words_, bits_, i, slot);
        }
    }
}

std::size_t PalettedContainer::distinctValueCount() const {
    const std::shared_lock<std::shared_mutex> guard{lock_};
    return paletteSize_;
}

std::size_t PalettedContainer::residentBytes() const {
    const std::shared_lock<std::shared_mutex> guard{lock_};
    return palette_.size() * sizeof(std::int32_t) + words_.size() * sizeof(std::uint64_t);
}

int PalettedContainer::bitsPerValue() const {
    const std::shared_lock<std::shared_mutex> guard{lock_};
    return bits_;
}

std::size_t PalettedContainer::indexOfOrAdd(std::int32_t value) {
    for (std::size_t i = 0; i < paletteSize_; ++i) {
        if (palette_[i] == value) {
            return i;
        }
    }
    if (paletteSize_ == palette_.size()) {
        palette_.resize(palette_.size() * 2, 0);
    }
    palette_[paletteSize_] = value;
    paletteSize_++;

    const int needed = bitsFor(paletteSize_);
    if (needed <= bits_) {
        return paletteSize_ - 1;
    }
    const int widened = std::max(needed, kMinBitsPerEntry);
    std::vector<std::uint64_t> words((size_ * static_cast<std::size_t>(widened) + 63) / 64, 0);
    // Palette slots are unchanged, so re-packing the indices at the wider width moves no values.
    if (!words_.empty()) {
        for (std::size_t i = 0; i < size_; ++i) {
            writeInto(words, widened, i, readRaw(words_, bits_, i));
        }
    }
    bits_ = widened;
    words_ = std::move(words);
    return paletteSize_ - 1;
}

} // namespace basalt::server::world
