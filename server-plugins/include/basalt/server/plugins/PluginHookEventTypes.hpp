#pragma once

#include <basalt/server/plugins/PluginEvents.hpp>

#include <vector>

namespace basalt::server::plugins::pluginEventTypes {
#define HOOK_EVENT(accessor, name, cancellable, fields, mutables) [[nodiscard]] const PluginEventType& accessor();
#define FIELDS(...)
#define MUTABLE(...)
#include <basalt/server/plugins/PluginHookEvents.def>
#undef MUTABLE
#undef FIELDS
#undef HOOK_EVENT

[[nodiscard]] const std::vector<const PluginEventType*>& hookEvents();
} // namespace basalt::server::plugins::pluginEventTypes
