#include <basalt/server/plugins/PluginHookEventTypes.hpp>

#include <string>

namespace basalt::server::plugins::pluginEventTypes {
#define FIELDS(...)                                                                                                    \
    std::vector<std::string> { __VA_ARGS__ }
#define MUTABLE(...)                                                                                                   \
    std::vector<std::string> { __VA_ARGS__ }
#define HOOK_EVENT(accessor, name, cancellable, fields, mutables)                                                      \
    const PluginEventType& accessor() {                                                                                \
        static const PluginEventType type{name, cancellable, fields, mutables};                                        \
        return type;                                                                                                   \
    }
#include <basalt/server/plugins/PluginHookEvents.def>
#undef HOOK_EVENT

const std::vector<const PluginEventType*>& hookEvents() {
    static const std::vector<const PluginEventType*> every{
#define HOOK_EVENT(accessor, name, cancellable, fields, mutables) &accessor(),
#include <basalt/server/plugins/PluginHookEvents.def>
#undef HOOK_EVENT
    };
    return every;
}

#undef MUTABLE
#undef FIELDS
} // namespace basalt::server::plugins::pluginEventTypes
