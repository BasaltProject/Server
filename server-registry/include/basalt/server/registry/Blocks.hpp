#pragma once

#include <basalt/server/registry/Registry.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace basalt::server::registry {
struct BlockState {
    wire::common::Identifier block;
    std::int32_t stateId = 0;
    std::map<std::string, std::string> properties;

    friend bool operator==(const BlockState&, const BlockState&) = default;
};

struct BlockHardness {
    float destroyTime = 0.0F;
    bool requiresCorrectTool = false;
    // parity: `Block.getExplosionResistance`, what an explosion has to spend to get through.
    float explosionResistance = 0.0F;

    [[nodiscard]] bool isInstant() const noexcept { return destroyTime == 0.0F; }

    [[nodiscard]] bool isUnbreakable() const noexcept { return destroyTime < 0.0F; }
};

namespace blocks {
[[nodiscard]] const Registry<BlockState>& registry();

[[nodiscard]] std::optional<std::int32_t> blockIdOf(const wire::common::Identifier& block);

[[nodiscard]] std::optional<std::int32_t> defaultStateIdOf(const wire::common::Identifier& block);

[[nodiscard]] const BlockHardness* hardnessOf(const wire::common::Identifier& block);

[[nodiscard]] std::optional<BlockState> byStateId(std::int32_t stateId);

// parity: `BlockStateParser`.
[[nodiscard]] std::optional<std::int32_t> stateIdOf(const wire::common::Identifier& block,
                                                    const std::map<std::string, std::string>& properties);

[[nodiscard]] std::int32_t stateCount();

// parity: BlockState.isAir, including the two non-default air blocks worldgen uses.
[[nodiscard]] bool isAir(std::int32_t stateId);

[[nodiscard]] const BlockState& air();
[[nodiscard]] const BlockState& stone();
[[nodiscard]] const BlockState& water();
[[nodiscard]] const BlockState& lava();
[[nodiscard]] const BlockState& bedrock();
[[nodiscard]] const BlockState& voidAir();
[[nodiscard]] const BlockState& caveAir();

[[nodiscard]] const BlockState* named(std::string_view path);
} // namespace blocks
} // namespace basalt::server::registry
