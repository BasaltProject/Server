#include <basalt/server/runtime/plugins/ServerControl.hpp>

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

} // namespace basalt::server::runtime::plugins
