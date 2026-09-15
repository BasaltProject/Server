#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/player/PlayerAnvil.hpp>
#include <basalt/server/runtime/player/PlayerBlockBreak.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
namespace {

namespace reg = ::basalt::server::registry;

using wire::common::Identifier;

/// parity: `AnvilBlock.DAMAGE`, the chain a used anvil falls down before it is gone.
[[nodiscard]] const Identifier* moreDamagedThan(const std::string_view block) {
    static const Identifier kChipped = Identifier::vanilla("chipped_anvil");
    static const Identifier kDamaged = Identifier::vanilla("damaged_anvil");
    if (block == "anvil") {
        return &kChipped;
    }
    if (block == "chipped_anvil") {
        return &kDamaged;
    }
    return nullptr;
}

[[nodiscard]] std::vector<world::BlockEffect> clang(const std::string_view name) {
    return {world::blockEffect::Sound{
        .name = std::string{name}, .source = wire::protocol::play::SoundSource::Blocks, .volume = 1.0F, .pitch = 1.0F}};
}

} // namespace

bool wearAnvilAt(const BreakContext& context, world::LoadedWorld& world, const std::int32_t x, const std::int32_t y,
                 const std::int32_t z, const float roll) {
    /// parity: `AnvilMenu.onTake`, the twelve hundredths that cost the block a step.
    constexpr float kBreakChance = 0.12F;

    const std::optional<std::int32_t> stateId = world.state().blockState(x, y, z);
    const Identifier* block = stateId.has_value() ? reg::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || context.blocks == nullptr) {
        return false;
    }
    const std::string_view path = block->path();
    if (path != "anvil" && path != "chipped_anvil" && path != "damaged_anvil") {
        return false;
    }
    const std::vector<PlayerSession*> watchers = playersIn(context, world.name());
    if (roll >= kBreakChance) {
        context.blocks->sendEffects(watchers, x, y, z, *stateId, clang("block.anvil.use"));
        return false;
    }
    const Identifier* worse = moreDamagedThan(path);
    const std::optional<std::int32_t> next =
        worse == nullptr
            ? std::optional<std::int32_t>{0}
            : reg::blockStateDefinitions::stateIdOf(*worse, reg::blockStateDefinitions::propertiesOf(*stateId));
    if (!next.has_value()) {
        return false;
    }
    world.state().setBlockState(x, y, z, *next);
    announceBlock(context, world.name(), x, y, z, *next);
    if (worse == nullptr) {
        notifyAround(context, world, x, y, z, stateId);
        context.blocks->sendEffects(watchers, x, y, z, 0, clang("block.anvil.destroy"));
        return true;
    }
    notifyAround(context, world, x, y, z, std::nullopt);
    context.blocks->sendEffects(watchers, x, y, z, *next, clang("block.anvil.use"));
    return false;
}

} // namespace basalt::server::runtime::player
