#pragma once

#include <basalt/server/runtime/world/RealBlockTags.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// Whether a state's block is in a tag, from a per-state table; the tags never change after startup.
class StateTagMemo {
public:
    explicit StateTagMemo(std::string_view tag) : tag_(tag) {}

    [[nodiscard]] bool contains(const realBlockTags::TagMembers& tags, std::int32_t stateId) const;

private:
    std::string tag_;
    mutable std::once_flag bound_;
    mutable const realBlockTags::TagMembers* tags_ = nullptr;
    mutable std::unique_ptr<std::atomic<std::uint8_t>[]> slots_;
    mutable std::int32_t size_ = 0;
};

} // namespace basalt::server::runtime::world
