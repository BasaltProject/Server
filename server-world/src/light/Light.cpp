#include "LightSpread.hpp"

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>

namespace basalt::server::world::light {

namespace skyLight {
namespace {

[[nodiscard]] std::int32_t uncachedOpacity(std::int32_t stateId) { return registry::blockShapes::dampening(stateId); }

[[nodiscard]] const std::vector<std::int32_t>& table() {
    static const std::vector<std::int32_t> built = [] {
        std::vector<std::int32_t> table(static_cast<std::size_t>(registry::blocks::stateCount()));
        for (std::size_t stateId = 0; stateId < table.size(); ++stateId) {
            table[stateId] = uncachedOpacity(static_cast<std::int32_t>(stateId));
        }
        return table;
    }();
    return built;
}

constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kSides = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

} // namespace

std::int32_t opacity(std::int32_t stateId) {
    const std::vector<std::int32_t>& built = table();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= built.size()) {
        return uncachedOpacity(stateId);
    }
    return built[static_cast<std::size_t>(stateId)];
}

std::span<const std::int32_t> opacityTable() { return table(); }

std::vector<SectionLight> compute(const LightWindow& window) {
    const auto [lowestSurface, highestSurface] = window.surfaceRange();
    const std::int32_t topY = std::clamp(highestSurface + 1, window.minY(), window.maxY());
    const std::int32_t bottomY = std::clamp(lowestSurface - kMax - 1, window.minY(), topY);
    LightSpread pass{window, bottomY, topY, kMax};

    // parity: `ChunkSkyLightSources`.
    std::vector<std::int32_t> floor(static_cast<std::size_t>(LightWindow::kWidth) * LightWindow::kWidth);
    for (std::int32_t z = 0; z < LightWindow::kWidth; ++z) {
        for (std::int32_t x = 0; x < LightWindow::kWidth; ++x) {
            std::int32_t lowest = topY + 1;
            for (std::int32_t y = topY; y >= bottomY; --y) {
                if (pass.opacityAt(x, y, z) > 0) {
                    break;
                }
                pass.set(x, y, z, kMax);
                lowest = y;
            }
            floor[static_cast<std::size_t>(z * LightWindow::kWidth + x)] = lowest;
        }
    }

    for (std::int32_t z = 0; z < LightWindow::kWidth; ++z) {
        for (std::int32_t x = 0; x < LightWindow::kWidth; ++x) {
            const std::int32_t lowest = floor[static_cast<std::size_t>(z * LightWindow::kWidth + x)];
            if (lowest > topY) {
                continue;
            }
            std::int32_t highestNeighbourFloor = lowest;
            for (const auto& [stepX, stepZ] : kSides) {
                const std::int32_t nextX = x + stepX;
                const std::int32_t nextZ = z + stepZ;
                if (nextX < 0 || nextX >= LightWindow::kWidth || nextZ < 0 || nextZ >= LightWindow::kWidth) {
                    continue;
                }
                highestNeighbourFloor = std::max(highestNeighbourFloor,
                                                 floor[static_cast<std::size_t>(nextZ * LightWindow::kWidth + nextX)]);
            }
            for (std::int32_t y = lowest; y <= std::min(highestNeighbourFloor, topY); ++y) {
                pass.push(x, y, z, kMax);
            }
        }
    }
    return pass.run();
}

} // namespace skyLight

namespace blockLight {
namespace {

/// parity: `Blocks`'s own `lightLevel(...)`.
[[nodiscard]] const std::vector<std::int32_t>& table() {
    static const std::vector<std::int32_t> built = [] {
        const std::array<std::pair<std::int32_t, std::vector<std::string_view>>, 10> flat = {{
            {15,
             {"beacon", "conduit", "copper_lantern", "end_gateway", "end_portal", "fire", "glowstone", "jack_o_lantern",
              "lantern", "lava", "lava_cauldron", "ochre_froglight", "pearlescent_froglight", "sea_lantern",
              "shroomlight", "verdant_froglight"}},
            {14, {"copper_torch", "copper_wall_torch", "end_rod", "torch", "wall_torch"}},
            {11, {"nether_portal"}},
            {10, {"crying_obsidian", "soul_fire", "soul_lantern", "soul_torch", "soul_wall_torch"}},
            {7, {"enchanting_table", "ender_chest"}},
            {6, {"sculk_catalyst"}},
            {5, {"amethyst_cluster"}},
            {4, {"large_amethyst_bud"}},
            {3, {"magma_block"}},
            {2, {"firefly_bush", "medium_amethyst_bud"}},
        }};
        /// parity: `litBlockEmission(n)`, which is `state -> state.getValue(LIT) ? n : 0`.
        const std::array<std::pair<std::int32_t, std::vector<std::string_view>>, 4> lit = {{
            {15, {"campfire", "redstone_lamp"}},
            {13, {"blast_furnace", "furnace", "smoker"}},
            {10, {"soul_campfire"}},
            {9, {"deepslate_redstone_ore", "redstone_ore"}},
        }};
        const std::array<std::pair<std::int32_t, std::vector<std::string_view>>, 2> one = {{
            {7, {"redstone_torch", "redstone_wall_torch"}},
            {1,
             {"brewing_stand", "brown_mushroom", "dragon_egg", "end_portal_frame", "sculk_sensor",
              "small_amethyst_bud"}},
        }};
        std::vector<std::int32_t> table(static_cast<std::size_t>(registry::blocks::stateCount()), 0);
        const auto eachState = [&table](const std::string_view name, const auto& answer) {
            const wire::common::Identifier block = wire::common::Identifier::vanilla(std::string{name});
            const registry::blockStateDefinitions::Definition* definition = registry::blockStateDefinitions::of(block);
            // A block with a single state is not in the definitions at all, so its own default is the whole of it.
            const std::int32_t first = definition != nullptr ? definition->firstStateId()
                                                             : registry::blocks::registry().idOf(block).value_or(-1);
            if (first < 0) {
                return;
            }
            const std::int32_t count = definition != nullptr ? definition->stateCount() : 1;
            for (std::int32_t stateId = first; stateId < first + count; ++stateId) {
                if (static_cast<std::size_t>(stateId) >= table.size()) {
                    continue;
                }
                table[static_cast<std::size_t>(stateId)] = answer(
                    definition != nullptr ? definition->propertiesOf(stateId) : std::map<std::string, std::string>{});
            }
        };
        const auto valueOf = [](const std::map<std::string, std::string>& properties, const std::string& name) {
            const auto found = properties.find(name);
            return found == properties.end() ? std::string{} : found->second;
        };
        const auto numberOf = [&valueOf](const std::map<std::string, std::string>& properties,
                                         const std::string& name) {
            const std::string text = valueOf(properties, name);
            std::int32_t parsed = 0;
            const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), parsed);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size() ? parsed : 0;
        };

        for (const auto& [level, names] : flat) {
            for (const std::string_view name : names) {
                eachState(name, [level](const std::map<std::string, std::string>&) { return level; });
            }
        }
        for (const auto& [level, names] : lit) {
            for (const std::string_view name : names) {
                eachState(name, [level, &valueOf](const std::map<std::string, std::string>& properties) {
                    return valueOf(properties, "lit") == "true" ? level : 0;
                });
            }
        }
        for (const auto& [level, names] : one) {
            for (const std::string_view name : names) {
                eachState(name, [level, &valueOf](const std::map<std::string, std::string>& properties) {
                    // parity: the two redstone torches are `litBlockEmission(7)` as well.
                    const std::string text = valueOf(properties, "lit");
                    return text.empty() || text == "true" ? level : 0;
                });
            }
        }
        // parity: `CandleBlock.LIGHT_EMISSION`, three per candle and only while they are lit.
        const auto everyBlockEnding = [&eachState](const std::string_view suffix, const auto& answer) {
            for (const std::int32_t id : registry::blocks::registry().ids()) {
                const wire::common::Identifier* block = registry::blocks::registry().nameOf(id);
                if (block != nullptr && block->path().ends_with(suffix)) {
                    eachState(block->path(), answer);
                }
            }
        };
        everyBlockEnding("candle", [&valueOf, &numberOf](const std::map<std::string, std::string>& properties) {
            return valueOf(properties, "lit") == "true" ? 3 * numberOf(properties, "candles") : 0;
        });
        // parity: `CANDLE_CAKE` and friends are `litBlockEmission(3)`, one candle and no count.
        everyBlockEnding("candle_cake", [&valueOf](const std::map<std::string, std::string>& properties) {
            return valueOf(properties, "lit") == "true" ? 3 : 0;
        });
        // parity: `COPPER_BULB`.
        const std::array<std::pair<std::int32_t, std::string_view>, 4> bulbs{{{15, "copper_bulb"},
                                                                              {12, "exposed_copper_bulb"},
                                                                              {8, "weathered_copper_bulb"},
                                                                              {4, "oxidized_copper_bulb"}}};
        for (const auto& [level, name] : bulbs) {
            for (const std::string_view prefix : {std::string_view{}, std::string_view{"waxed_"}}) {
                eachState(std::string{prefix} + std::string{name},
                          [level, &valueOf](const std::map<std::string, std::string>& properties) {
                              return valueOf(properties, "lit") == "true" ? level : 0;
                          });
            }
        }
        // parity: `GlowLichenBlock.emission(7)`.
        eachState("glow_lichen", [&valueOf](const std::map<std::string, std::string>& properties) {
            static constexpr std::array<std::string_view, 6> kFaces{"down", "up", "north", "south", "west", "east"};
            return std::ranges::any_of(
                       kFaces,
                       [&](const std::string_view face) { return valueOf(properties, std::string{face}) == "true"; })
                       ? 7
                       : 0;
        });
        for (const std::string_view vines : {"cave_vines", "cave_vines_plant"}) {
            eachState(vines, [&valueOf](const std::map<std::string, std::string>& properties) {
                return valueOf(properties, "berries") == "true" ? 14 : 0;
            });
        }
        // parity: `SeaPickleBlock`, where a pickle out of water is dead and gives nothing.
        eachState("sea_pickle", [&valueOf, &numberOf](const std::map<std::string, std::string>& properties) {
            return valueOf(properties, "waterlogged") == "true" ? 3 + (3 * numberOf(properties, "pickles")) : 0;
        });
        // parity: `RespawnAnchorBlock.getScaledChargeLevel(state, 15)`.
        eachState("respawn_anchor", [&numberOf](const std::map<std::string, std::string>& properties) {
            return static_cast<std::int32_t>(static_cast<float>(numberOf(properties, "charges")) / 4.0F * 15.0F);
        });
        // parity: `LightBlock.LIGHT_EMISSION`, which is the level it was given.
        eachState("light", [&numberOf](const std::map<std::string, std::string>& properties) {
            return numberOf(properties, "level");
        });
        // parity: `TrialSpawnerState.lightLevel` and `VaultState.lightLevel`.
        eachState("trial_spawner", [&valueOf](const std::map<std::string, std::string>& properties) {
            const std::string state = valueOf(properties, "trial_spawner_state");
            if (state == "waiting_for_players") {
                return 4;
            }
            return state == "active" || state == "waiting_for_reward_ejection" || state == "ejecting_reward" ? 8 : 0;
        });
        eachState("vault", [&valueOf](const std::map<std::string, std::string>& properties) {
            return valueOf(properties, "vault_state") == "inactive" ? 6 : 12;
        });
        return table;
    }();
    return built;
}

} // namespace

std::int32_t emission(std::int32_t stateId) {
    const std::vector<std::int32_t>& built = table();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= built.size()) {
        return 0;
    }
    return built[static_cast<std::size_t>(stateId)];
}

std::optional<std::vector<SectionLight>> compute(const LightWindow& window) {
    const std::optional<std::pair<std::size_t, std::size_t>> sections = window.emitterSections(emission);
    if (!sections.has_value()) {
        return std::nullopt;
    }
    const auto [first, last] = *sections;
    const std::int32_t bottomY = std::max(window.minY() + (static_cast<std::int32_t>(first) - 1) * 16, window.minY());
    const std::int32_t topY = std::min(window.minY() + (static_cast<std::int32_t>(last) + 2) * 16 - 1, window.maxY());
    LightSpread pass{window, bottomY, topY, 0};
    window.forEachEmitter(
        emission, bottomY, topY,
        [&pass](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t level) { pass.seed(x, y, z, level); });
    return pass.run();
}

} // namespace blockLight

} // namespace basalt::server::world::light
