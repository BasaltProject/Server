#include <basalt/server/runtime/entity/IntHashOrder.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace basalt::server::runtime::entity {
namespace {

constexpr float kLoadFactor = 0.75F;
/// parity: the no-argument constructor's sixteen expected entries.
constexpr std::int32_t kExpected = 16;

/// parity: `HashCommon.arraySize`.
[[nodiscard]] std::int32_t arraySize(std::int32_t expected) {
    const auto needed = static_cast<std::uint32_t>(std::ceil(static_cast<float>(expected) / kLoadFactor));
    return std::max<std::int32_t>(2, static_cast<std::int32_t>(std::bit_ceil(needed)));
}

/// parity: `HashCommon.maxFill`.
[[nodiscard]] std::int32_t maxFill(std::int32_t capacity) {
    return std::min(static_cast<std::int32_t>(std::ceil(static_cast<float>(capacity) * kLoadFactor)), capacity - 1);
}

} // namespace

IntHashOrder::IntHashOrder() {
    const std::int32_t capacity = arraySize(kExpected);
    keys_.assign(static_cast<std::size_t>(capacity), 0);
    mask_ = capacity - 1;
    maxFill_ = maxFill(capacity);
}

std::uint32_t IntHashOrder::mix(std::int32_t key) {
    const std::uint32_t hash = static_cast<std::uint32_t>(key) * 0x9E3779B9U;
    return hash ^ (hash >> 16U);
}

void IntHashOrder::insert(std::int32_t key) {
    auto pos = static_cast<std::int32_t>(mix(key) & static_cast<std::uint32_t>(mask_));
    while (keys_[static_cast<std::size_t>(pos)] != 0) {
        if (keys_[static_cast<std::size_t>(pos)] == key) {
            return;
        }
        pos = (pos + 1) & mask_;
    }
    keys_[static_cast<std::size_t>(pos)] = key;
    if (size_++ >= maxFill_) {
        rehash(arraySize(size_ + 1));
    }
}

void IntHashOrder::remove(std::int32_t key) {
    auto pos = static_cast<std::int32_t>(mix(key) & static_cast<std::uint32_t>(mask_));
    while (keys_[static_cast<std::size_t>(pos)] != 0) {
        if (keys_[static_cast<std::size_t>(pos)] == key) {
            --size_;
            shiftKeys(pos, nullptr);
            return;
        }
        pos = (pos + 1) & mask_;
    }
}

void IntHashOrder::shiftKeys(std::int32_t pos, std::vector<std::int32_t>* wrapped) {
    for (;;) {
        const std::int32_t last = pos;
        pos = (last + 1) & mask_;
        std::int32_t current = 0;
        for (;;) {
            current = keys_[static_cast<std::size_t>(pos)];
            if (current == 0) {
                keys_[static_cast<std::size_t>(last)] = 0;
                return;
            }
            const auto slot = static_cast<std::int32_t>(mix(current) & static_cast<std::uint32_t>(mask_));
            if (last <= pos ? (last >= slot || slot > pos) : (last >= slot && slot > pos)) {
                break;
            }
            pos = (pos + 1) & mask_;
        }
        if (wrapped != nullptr && pos < last) {
            wrapped->push_back(current);
        }
        keys_[static_cast<std::size_t>(last)] = current;
    }
}

void IntHashOrder::rehash(std::int32_t capacity) {
    std::vector<std::int32_t> fresh(static_cast<std::size_t>(capacity), 0);
    const std::int32_t mask = capacity - 1;
    auto from = static_cast<std::int32_t>(keys_.size());
    for (std::int32_t left = size_; left-- != 0;) {
        while (keys_[static_cast<std::size_t>(--from)] == 0) {
        }
        const std::int32_t key = keys_[static_cast<std::size_t>(from)];
        auto pos = static_cast<std::int32_t>(mix(key) & static_cast<std::uint32_t>(mask));
        while (fresh[static_cast<std::size_t>(pos)] != 0) {
            pos = (pos + 1) & mask;
        }
        fresh[static_cast<std::size_t>(pos)] = key;
    }
    keys_ = std::move(fresh);
    mask_ = mask;
    maxFill_ = maxFill(capacity);
}

std::vector<std::int32_t> IntHashOrder::order() const {
    std::vector<std::int32_t> found;
    for (auto pos = static_cast<std::int32_t>(keys_.size()); pos-- > 0;) {
        if (keys_[static_cast<std::size_t>(pos)] != 0) {
            found.push_back(keys_[static_cast<std::size_t>(pos)]);
        }
    }
    return found;
}

void IntHashOrder::iterate(const std::function<bool(std::int32_t key)>& visit) {
    // parity: `MapIterator`: downwards from the top, then whatever a removal wrapped round.
    auto pos = static_cast<std::int32_t>(keys_.size());
    std::int32_t remaining = size_;
    std::vector<std::int32_t> wrapped;
    while (remaining != 0) {
        --remaining;
        std::int32_t last = 0;
        std::int32_t key = 0;
        for (;;) {
            if (--pos < 0) {
                key = wrapped[static_cast<std::size_t>(-pos - 1)];
                last = std::numeric_limits<std::int32_t>::min();
                break;
            }
            if (keys_[static_cast<std::size_t>(pos)] != 0) {
                last = pos;
                key = keys_[static_cast<std::size_t>(pos)];
                break;
            }
        }
        if (!visit(key)) {
            continue;
        }
        if (pos >= 0) {
            shiftKeys(last, &wrapped);
            --size_;
        } else {
            remove(key);
        }
    }
}

} // namespace basalt::server::runtime::entity
