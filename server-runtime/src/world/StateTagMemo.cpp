#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/StateTagMemo.hpp>

namespace basalt::server::runtime::world {

namespace {

constexpr std::uint8_t kOut = 1;
constexpr std::uint8_t kIn = 2;

[[nodiscard]] bool resolve(const realBlockTags::TagMembers& tags, std::string_view tag, std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block != nullptr && realBlockTags::contains(tags, tag, *block);
}

} // namespace

bool StateTagMemo::contains(const realBlockTags::TagMembers& tags, std::int32_t stateId) const {
    std::call_once(bound_, [this, &tags] {
        size_ = registry::blockStateDefinitions::stateIdBound();
        slots_ = std::make_unique<std::atomic<std::uint8_t>[]>(static_cast<std::size_t>(size_));
        tags_ = &tags;
    });
    if (tags_ != &tags || stateId < 0 || stateId >= size_) {
        return resolve(tags, tag_, stateId);
    }
    std::atomic<std::uint8_t>& slot = slots_[static_cast<std::size_t>(stateId)];
    const std::uint8_t known = slot.load(std::memory_order_relaxed);
    if (known != 0) {
        return known == kIn;
    }
    const bool in = resolve(tags, tag_, stateId);
    slot.store(in ? kIn : kOut, std::memory_order_relaxed);
    return in;
}

} // namespace basalt::server::runtime::world
