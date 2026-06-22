#include <basalt/server/runtime/plugins/ServerWorldEnv.hpp>
#include <basalt/server/runtime/registry/VanillaBiomes.hpp>

namespace basalt::server::runtime::plugins {

float biomeDownfall(const std::string_view path) {
    for (const registry::vanillaBiomes::Entry& entry : registry::vanillaBiomes::all()) {
        if (entry.path == path) {
            return entry.downfall;
        }
    }
    return 0.4F;
}

} // namespace basalt::server::runtime::plugins
