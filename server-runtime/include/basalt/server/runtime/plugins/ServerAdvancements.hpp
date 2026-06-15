#pragma once

#include <basalt/server/plugins/PluginAdvancements.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/AdvancementTree.hpp>
#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace basalt::server::runtime::plugins {

class ServerAdvancements final : public server::plugins::PluginAdvancements {
public:
    struct Hooks {
        std::function<const world::RealAdvancements*()> advancements;
        std::function<world::PlayerProgress*()> progress;
        std::function<world::AdvancementTriggers*()> triggers;
        std::function<std::vector<player::PlayerSession*>()> sessions;
    };

    explicit ServerAdvancements(Hooks hooks) : hooks_(std::move(hooks)) {}

    [[nodiscard]] std::vector<std::string> ids() override;
    [[nodiscard]] std::optional<server::plugins::PluginAdvancement> advancement(std::string_view id) override;
    [[nodiscard]] std::map<std::string, std::int64_t> obtained(std::string_view uuid, std::string_view id) override;
    bool grant(std::string_view uuid, std::string_view id, std::string_view criterion) override;
    bool revoke(std::string_view uuid, std::string_view id, std::string_view criterion) override;

private:
    [[nodiscard]] const world::AdvancementTree& tree();

    Hooks hooks_;
    std::unique_ptr<world::AdvancementTree> tree_;
    const world::RealAdvancements* treeOf_ = nullptr;
};

} // namespace basalt::server::runtime::plugins
