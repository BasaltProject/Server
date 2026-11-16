#pragma once

#include <basalt/server/runtime/VaultSystem.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>

namespace basalt::server::runtime::plugins {

/// The three events a vault asks the plugins about, bound to `sink`, which must outlive the system.
[[nodiscard]] VaultSystem::Hooks vaultHooks(const EventSink& sink);

} // namespace basalt::server::runtime::plugins
