#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// What an axe, a hoe or a shovel turns a block into, read from `data/<ns>/block_transformer/*.json`.
/// parity: `BlockTransformer` over `RuleBasedStateProvider`, with the predicates and providers the
/// vanilla files use. Anything else in a file makes that one transform never apply.
class RealBlockTransformers {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// Whether a block is in a block tag, `minecraft:` spelled out.
    using Tagged = std::function<bool(std::string_view tag, const wire::common::Identifier& block)>;

    /// parity: `BlockTransformer.TransformParticle`, as the level event it sends.
    enum class Particle : std::uint8_t { None, Scrape, WaxOn, WaxOff };

    /// parity: `BlockTransformer.DropStrategy`.
    enum class Drop : std::uint8_t { FromMiddle, ClickedFace };

    struct Provider;

    /// parity: `BlockTransformer.BlockTransformData`, with the codec's defaults.
    struct Transform {
        std::shared_ptr<const Provider> provider;
        std::string sound;
        Particle particle = Particle::None;
        std::vector<wire::protocol::play::Direction> disallowedFaces;
        std::optional<wire::common::Identifier> loot;
        Drop drop = Drop::FromMiddle;
        bool updateFromNeighbors = true;
        bool copperChest = false;
        bool consumeOnUse = true;
        std::int32_t itemDamagePerUse = 0;
    };

    explicit RealBlockTransformers(ReadText readText) : readText_(std::move(readText)) {}

    /// The transforms of `id`, in file order; empty for one the jar does not have.
    [[nodiscard]] const std::vector<Transform>& of(const wire::common::Identifier& id);

    /// Parses one file, so the shape can be pinned without a jar.
    [[nodiscard]] static std::vector<Transform> parse(std::string_view json);

    /// parity: `BlockStateProvider.getOptionalState` over the rules: the state the block becomes, or
    /// nothing when no rule matches.
    [[nodiscard]] static std::optional<std::int32_t> stateFor(const Transform& transform, const WorldState& world,
                                                              std::int32_t x, std::int32_t y, std::int32_t z,
                                                              const Tagged& tagged);

    /// The transformer an item carries by default. parity: `Item.Properties.axe/hoe/shovel`.
    [[nodiscard]] static std::optional<wire::common::Identifier> defaultFor(const wire::common::Identifier& item);

private:
    ReadText readText_;
    // Several connection threads use tools at once; the cache has one owner and it is this lock.
    // Map nodes never move, so a list handed out stays valid.
    std::mutex mutex_;
    std::map<wire::common::Identifier, std::vector<Transform>> cache_;
};

} // namespace basalt::server::runtime::world
