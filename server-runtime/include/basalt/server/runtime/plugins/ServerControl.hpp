#pragma once

#include <basalt/server/plugins/PluginServerControl.hpp>
#include <basalt/server/runtime/TickLoop.hpp>
#include <basalt/server/runtime/plugins/ServerSettings.hpp>

#include <functional>
#include <mutex>

namespace basalt::server::runtime::plugins {

class ServerControl final : public server::plugins::PluginServerControl {
public:
    struct Hooks {
        std::function<TickLoop*()> tickLoop;
        std::string bugReportLink;
        std::function<std::optional<server::plugins::PluginResourcePack>()> resourcePack;
        ServerSettings* settings = nullptr;
    };

    explicit ServerControl(Hooks hooks);

    [[nodiscard]] float tickRate() override;
    [[nodiscard]] std::array<double, 4> pace() override;
    void setTickRate(float rate) override;
    [[nodiscard]] bool frozen() override;
    void setFrozen(bool value) override;
    bool step(std::int32_t ticks) override;
    [[nodiscard]] std::int32_t stepsOwed() override;
    bool stopStepping() override;
    bool sprint(std::int32_t ticks) override;
    [[nodiscard]] bool sprinting() override;
    bool stopSprinting() override;
    [[nodiscard]] std::vector<server::plugins::PluginServerLink> links() override;
    void setLinks(std::vector<server::plugins::PluginServerLink> links) override;
    [[nodiscard]] std::optional<server::plugins::PluginResourcePack> resourcePack() override;
    [[nodiscard]] std::vector<std::string> settingKeys() override;
    [[nodiscard]] std::optional<std::string> setting(std::string_view key) override;
    bool setSetting(std::string_view key, std::string_view value) override;

private:
    Hooks hooks_;
    std::mutex linksMutex_;
    std::vector<server::plugins::PluginServerLink> links_;
};

} // namespace basalt::server::runtime::plugins
