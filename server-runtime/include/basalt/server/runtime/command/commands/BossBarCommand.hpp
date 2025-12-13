#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

#include <map>
#include <set>
#include <vector>

namespace basalt::server::runtime::command::commands {

/// One `/bossbar` bar: what it says, how full it is, and who can see it.
struct CustomBossBar {
    wire::common::Identifier id;
    wire::text::Component name;
    wire::common::Uuid uuid = wire::common::Uuid::random();
    wire::protocol::play::BossBarColor color = wire::protocol::play::BossBarColor::White;
    wire::protocol::play::BossBarOverlay overlay = wire::protocol::play::BossBarOverlay::Progress;
    std::int32_t value = 0;
    std::int32_t max = 100;
    bool visible = true;
    /// Player names rather than sessions: a bar outlives any one connection, as vanilla's does.
    std::set<std::string> players;

    [[nodiscard]] float progress() const;

    [[nodiscard]] wire::protocol::play::BossEventPacket::Add addOperation() const;
};

/// Bars by id, and the packets that keep clients in step with them.
///
/// Held by the server rather than by the command, so a bar survives a `/reload` and so the join
/// path can show a returning player the bars they belong to. They are not persisted across a
/// restart yet, which vanilla does in `level.dat`; stated rather than silently lost.
class BossBarRegistry {
public:
    [[nodiscard]] std::vector<CustomBossBar*> all();

    [[nodiscard]] std::vector<const CustomBossBar*> all() const;

    [[nodiscard]] CustomBossBar* get(const wire::common::Identifier& id);

    void add(CustomBossBar bar);

    bool remove(const wire::common::Identifier& id);

    /// Everything a player should see the moment they join, which is every visible bar naming them.
    [[nodiscard]] std::vector<const CustomBossBar*> barsFor(const player::PlayerSession& player) const;

private:
    /// In insertion order, which is what `/bossbar list` prints and vanilla's own map keeps.
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
