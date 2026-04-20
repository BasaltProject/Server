#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::entity::ecs {
template <typename T>
class ComponentStore {
public:
    [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

    [[nodiscard]] bool has(std::int32_t entityIndex) const noexcept {
        return entityIndex >= 0 && static_cast<std::size_t>(entityIndex) < sparse_.size() &&
               sparse_[static_cast<std::size_t>(entityIndex)] != kAbsent;
    }

    [[nodiscard]] const T* get(std::int32_t entityIndex) const noexcept {
        if (!has(entityIndex)) {
            return nullptr;
        }
        return &values_[static_cast<std::size_t>(sparse_[static_cast<std::size_t>(entityIndex)])];
    }

    [[nodiscard]] T* get(std::int32_t entityIndex) noexcept {
        if (!has(entityIndex)) {
            return nullptr;
        }
        return &values_[static_cast<std::size_t>(sparse_[static_cast<std::size_t>(entityIndex)])];
    }

    void set(std::int32_t entityIndex, T value) {
        ensureSparseCapacity(entityIndex + 1);
        if (has(entityIndex)) {
            values_[static_cast<std::size_t>(sparse_[static_cast<std::size_t>(entityIndex)])] = std::move(value);
            return;
        }
        sparse_[static_cast<std::size_t>(entityIndex)] = static_cast<std::int32_t>(values_.size());
        dense_.push_back(entityIndex);
        values_.push_back(std::move(value));
    }

    void remove(std::int32_t entityIndex) {
        if (!has(entityIndex)) {
            return;
        }
        const std::size_t slot = static_cast<std::size_t>(sparse_[static_cast<std::size_t>(entityIndex)]);
        const std::size_t lastSlot = values_.size() - 1;
        const std::int32_t lastEntity = dense_[lastSlot];
        dense_[slot] = lastEntity;
        values_[slot] = std::move(values_[lastSlot]);
        sparse_[static_cast<std::size_t>(lastEntity)] = static_cast<std::int32_t>(slot);
        sparse_[static_cast<std::size_t>(entityIndex)] = kAbsent;
        dense_.pop_back();
        values_.pop_back();
    }

    template <typename Action>
    void forEach(Action&& action) {
        for (std::size_t slot = 0; slot < values_.size(); ++slot) {
            action(dense_[slot], values_[slot]);
        }
    }

private:
    static constexpr std::int32_t kAbsent = -1;

    void ensureSparseCapacity(std::int32_t minSize) {
        if (static_cast<std::size_t>(minSize) <= sparse_.size()) {
            return;
        }
        sparse_.resize(std::max(static_cast<std::size_t>(minSize), sparse_.size() * 2 + 16), kAbsent);
    }

    std::vector<std::int32_t> sparse_;
    std::vector<std::int32_t> dense_;
    std::vector<T> values_;
};
} // namespace basalt::server::entity::ecs
