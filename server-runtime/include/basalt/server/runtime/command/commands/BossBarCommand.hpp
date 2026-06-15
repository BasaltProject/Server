#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace basalt::server::runtime::command::commands {

struct CustomBossBar {
    wire::common::Identifier id;
    wire::text::Component name;
    wire::common::Uuid uuid = wire::common::Uuid::random();
    wire::protocol::play::BossBarColor color = wire::protocol::play::BossBarColor::White;
    wire::protocol::play::BossBarOverlay overlay = wire::protocol::play::BossBarOverlay::Progress;
    std::int32_t value = 0;
    std::int32_t max = 100;
    bool visible = true;
    bool darkenScreen = false;
    bool playMusic = false;
    bool createFog = false;
    std::set<std::string> players;

    [[nodiscard]] float progress() const;

    [[nodiscard]] wire::protocol::play::BossEventPacket::Add addOperation() const;
};

class BossBarRegistry {
public:
    [[nodiscard]] std::vector<CustomBossBar*> all();

    [[nodiscard]] std::vector<const CustomBossBar*> all() const;

    [[nodiscard]] CustomBossBar* get(const wire::common::Identifier& id);

    void add(CustomBossBar bar);

    bool remove(const wire::common::Identifier& id);

    [[nodiscard]] std::vector<const CustomBossBar*> barsFor(const player::PlayerSession& player) const;

    /// parity: the `custom_boss_events` saved data, one `CustomBossEvent.Packed` per bar; players are kept by name.
    [[nodiscard]] wire::nbt::NbtCompound encode() const;

    void restore(const wire::nbt::NbtCompound& packed);

private:
    std::vector<wire::common::Identifier> order_;
    std::map<wire::common::Identifier, CustomBossBar> bars_;
};

/// parity: `BossBarCommands`.
class BossBarCommand final : public Command {
public:
    explicit BossBarCommand(BossBarRegistry& registry);

    void build(const CommandNodeBuilder& root) const override;

private:
    BossBarRegistry* registry_;
};

} // namespace basalt::server::runtime::command::commands
