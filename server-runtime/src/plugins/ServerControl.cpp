#include <basalt/server/runtime/plugins/ServerControl.hpp>

#include <chrono>

namespace basalt::server::runtime::plugins {

ServerControl::ServerControl(Hooks hooks) : hooks_(std::move(hooks)) {
    if (hooks_.bugReportLink.starts_with("http://") || hooks_.bugReportLink.starts_with("https://")) {
        links_.push_back(server::plugins::PluginServerLink{.kind = "bug_report", .url = hooks_.bugReportLink});
    }
}

float ServerControl::tickRate() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop == nullptr ? 20.0F : loop->tickRate();
}

std::array<double, 4> ServerControl::pace() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    if (loop == nullptr) {
        return {20.0, 20.0, 20.0, 0.0};
    }
    const TickRates rates = loop->tickRates();
    const std::int64_t now =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    return {rates.oneMinute, rates.fiveMinutes, rates.fifteenMinutes, loop->durations().summary(now, 60).meanMillis};
}

std::vector<std::int64_t> ServerControl::tickTimes() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop == nullptr ? std::vector<std::int64_t>{} : loop->recentTickNanos();
}

bool ServerControl::paused() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop != nullptr && loop->isPaused();
}

void ServerControl::setTickRate(float rate) {
    if (TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr; loop != nullptr) {
        loop->setTickRate(rate);
    }
}

bool ServerControl::frozen() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop != nullptr && loop->isFrozen();
}

void ServerControl::setFrozen(bool value) {
    if (TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr; loop != nullptr) {
        loop->setFrozen(value);
    }
}

bool ServerControl::step(std::int32_t ticks) {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop != nullptr && loop->step(ticks);
}

std::int32_t ServerControl::stepsOwed() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop == nullptr ? 0 : loop->stepsOwed();
}

bool ServerControl::stopStepping() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop != nullptr && loop->stopStepping();
}

bool ServerControl::sprint(std::int32_t ticks) {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    if (loop == nullptr) {
        return false;
    }
    loop->sprint(ticks);
    return true;
}

bool ServerControl::sprinting() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop != nullptr && loop->isSprinting();
}

bool ServerControl::stopSprinting() {
    TickLoop* loop = hooks_.tickLoop ? hooks_.tickLoop() : nullptr;
    return loop != nullptr && loop->stopSprinting();
}

std::vector<server::plugins::PluginServerLink> ServerControl::links() {
    const std::lock_guard<std::mutex> lock{linksMutex_};
    return links_;
}

void ServerControl::setLinks(std::vector<server::plugins::PluginServerLink> links) {
    const std::lock_guard<std::mutex> lock{linksMutex_};
    links_ = std::move(links);
}

std::optional<server::plugins::PluginResourcePack> ServerControl::resourcePack() {
    return hooks_.resourcePack ? hooks_.resourcePack() : std::nullopt;
}

std::vector<std::uint8_t> ServerControl::serverIcon() {
    return hooks_.serverIcon ? hooks_.serverIcon() : std::vector<std::uint8_t>{};
}

bool ServerControl::reloadCommands() { return hooks_.reloadCommands && hooks_.reloadCommands(); }

std::vector<std::string> ServerControl::settingKeys() {
    return hooks_.settings == nullptr ? std::vector<std::string>{} : hooks_.settings->keys();
}

std::optional<std::string> ServerControl::setting(const std::string_view key) {
    return hooks_.settings == nullptr ? std::nullopt : hooks_.settings->get(key);
}

bool ServerControl::setSetting(const std::string_view key, const std::string_view value) {
    return hooks_.settings != nullptr && hooks_.settings->set(key, value);
}

} // namespace basalt::server::runtime::plugins
