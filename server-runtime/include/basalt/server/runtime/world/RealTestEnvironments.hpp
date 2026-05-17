#pragma once

#include <basalt/server/runtime/world/RealTestInstances.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <functional>
#include <map>

namespace basalt::server::runtime {
namespace command {
class CommandEnvironment;
}

namespace world {
class LoadedWorld;
}
} // namespace basalt::server::runtime

namespace basalt::server::runtime::world {

/// parity: `TestEnvironmentDefinition`: what a test changes about the world before it runs and puts back after.
class RealTestEnvironments {
public:
    explicit RealTestEnvironments(const RealTestInstances::Jar& jar,
                                  wire::common::Logger logger = wire::common::Logger{"Tests"});

    [[nodiscard]] std::function<void()> activate(const wire::common::JsonValue& definition, LoadedWorld& world,
                                                 const command::CommandEnvironment& environment) const;

private:
    [[nodiscard]] std::function<void()> apply(const wire::common::JsonValue& definition, LoadedWorld& world,
                                              const command::CommandEnvironment& environment, int depth) const;

    wire::common::Logger logger_;
    std::map<wire::common::Identifier, wire::common::JsonValue> named_;
};

} // namespace basalt::server::runtime::world
