#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace basalt::server::registry {
template <typename T>
class Registry {
public:
    const T* registerEntry(std::int32_t id, wire::common::Identifier identifier, T value) {
        if (valueById_.contains(id) || idByName_.contains(identifier)) {
            return nullptr;
        }
        const auto [entry, inserted] = valueById_.emplace(id, std::move(value));
        nameById_.emplace(id, identifier);
        idByName_.emplace(std::move(identifier), id);
        order_.push_back(id);
        return &entry->second;
    }

    [[nodiscard]] const T* byId(std::int32_t id) const {
        const auto found = valueById_.find(id);
        return found == valueById_.end() ? nullptr : &found->second;
    }

    [[nodiscard]] const T* byName(const wire::common::Identifier& identifier) const {
        const auto found = idByName_.find(identifier);
        return found == idByName_.end() ? nullptr : byId(found->second);
    }

    [[nodiscard]] std::optional<std::int32_t> idOf(const wire::common::Identifier& identifier) const {
        const auto found = idByName_.find(identifier);
        return found == idByName_.end() ? std::nullopt : std::optional{found->second};
    }

    [[nodiscard]] const wire::common::Identifier* nameOf(std::int32_t id) const {
        const auto found = nameById_.find(id);
        return found == nameById_.end() ? nullptr : &found->second;
    }

    [[nodiscard]] std::size_t size() const noexcept { return valueById_.size(); }

    [[nodiscard]] const std::vector<std::int32_t>& ids() const noexcept { return order_; }

private:
    std::unordered_map<std::int32_t, T> valueById_;
    std::unordered_map<wire::common::Identifier, std::int32_t> idByName_;
    std::unordered_map<std::int32_t, wire::common::Identifier> nameById_;
    std::vector<std::int32_t> order_;
};
} // namespace basalt::server::registry
