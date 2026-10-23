#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::plugins {

struct PluginParticle {
    std::string type;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::int32_t count = 1;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    float offsetZ = 0.0F;
    std::array<float, 3> speed{};
    /// 0 default, 1 alternative, 2 alternative with speed (`ClientboundLevelParticlesPacket.RandomizationType`).
    std::int32_t randomization = 0;
    bool force = false;
    /// Numbers the particle's options read in order: a state id, a colour, a scale, a target.
    std::vector<double> data;
    /// The item id of an item particle.
    std::string item;
    /// The player who alone sees it, or empty for everyone in range.
    std::string receiver;
};

struct PluginBorder {
    double centerX = 0.0;
    double centerZ = 0.0;
    double size = 0.0;
    double targetSize = 0.0;
    std::int64_t lerpTicks = 0;
    double damagePerBlock = 0.0;
    double damageBuffer = 0.0;
    std::int32_t warningBlocks = 0;
    std::int32_t warningSeconds = 0;
};

struct PluginDrop {
    std::string item;
    std::int32_t count = 0;
    /// The stack's components as SNBT.
    std::string components;
};

/// What a plugin can do to the physical world around it that the block and entity services do not cover.
class PluginWorldEnv {
public:
    PluginWorldEnv() = default;
    PluginWorldEnv(const PluginWorldEnv&) = delete;
    PluginWorldEnv& operator=(const PluginWorldEnv&) = delete;
    PluginWorldEnv(PluginWorldEnv&&) = delete;
    PluginWorldEnv& operator=(PluginWorldEnv&&) = delete;
    virtual ~PluginWorldEnv() = default;

    /// Plays a sound id to the players in range of the point, 16 blocks scaled by the volume.
    virtual void sound(std::string_view sound, std::string_view category, double x, double y, double z, float volume,
                       float pitch, std::int64_t seed) {
        static_cast<void>(sound);
        static_cast<void>(category);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(volume);
        static_cast<void>(pitch);
        static_cast<void>(seed);
    }

    virtual void particle(const PluginParticle& particle) { static_cast<void>(particle); }

    /// A level event (a sound or a burst of particles the client knows by number); `global` reaches every player.
    virtual void levelEvent(std::int32_t type, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t data,
                            bool global) {
        static_cast<void>(type);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(data);
        static_cast<void>(global);
    }

    /// A block event (a chest lid, a bell, a note) to the players watching the block; the numbers mean what the block
    /// says.
    virtual void blockEvent(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t first, std::int32_t second) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(first);
        static_cast<void>(second);
    }

    [[nodiscard]] virtual PluginBorder border() { return {}; }

    /// Applies the border settings that differ from the current ones; the size moves over `seconds` when positive.
    virtual void setBorder(const PluginBorder& border, double seconds) {
        static_cast<void>(border);
        static_cast<void>(seconds);
    }

    [[nodiscard]] virtual std::int32_t playerCount() { return 0; }

    /// The loaded block entities of the world.
    [[nodiscard]] virtual std::int32_t blockEntityCount() { return 0; }

    /// What the block at the position drops when broken with `tool` (an item id, empty for a hand), no stack spawned.
    [[nodiscard]] virtual std::optional<std::vector<PluginDrop>>
    blockDrops(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view tool,
               const std::vector<std::pair<std::string, std::int32_t>>& enchantments, bool byEntity) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(tool);
        static_cast<void>(enchantments);
        static_cast<void>(byEntity);
        return std::nullopt;
    }

    /// Breaks the block as a player would and drops what it gives; false when nothing broke.
    virtual bool breakBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::string_view tool,
                            const std::vector<std::pair<std::string, std::int32_t>>& enchantments, bool drops) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(tool);
        static_cast<void>(enchantments);
        static_cast<void>(drops);
        return false;
    }

    /// Spends bone meal on the block as a dispenser would; false when nothing there grows.
    virtual bool boneMeal(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return false;
    }

    /// The redstone signal a block gives toward `face` (0 to 5, down up north south west east), 0 to 15.
    [[nodiscard]] virtual std::int32_t signalFrom(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t face,
                                                  bool direct) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(face);
        static_cast<void>(direct);
        return 0;
    }

    /// The climate temperature at a point, as the biome there gives it at that height.
    [[nodiscard]] virtual double temperature(std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return 0.8;
    }

    /// How much of the block a player mines per tick with what they hold, `ServerPlayer.getDestroyProgress`.
    [[nodiscard]] virtual double breakSpeed(std::string_view player, std::int32_t x, std::int32_t y, std::int32_t z) {
        static_cast<void>(player);
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        return 0.0;
    }

    /// Whether the state would survive standing at the position (`BlockState.canSurvive`).
    [[nodiscard]] virtual bool survives(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        static_cast<void>(x);
        static_cast<void>(y);
        static_cast<void>(z);
        static_cast<void>(stateId);
        return true;
    }
};

} // namespace basalt::server::plugins
