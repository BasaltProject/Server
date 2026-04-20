#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/protocol/play/SpawnInfo.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace basalt::server::runtime::world {
class LoadedWorld;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::command {

[[nodiscard]] std::shared_ptr<const ArgumentType> wordArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> stringArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> boolArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> doubleArgument(double min = -std::numeric_limits<double>::max(),
                                                                 double max = std::numeric_limits<double>::max());

[[nodiscard]] std::shared_ptr<const ArgumentType> floatArgument(float min = -std::numeric_limits<float>::max(),
                                                                float max = std::numeric_limits<float>::max());

[[nodiscard]] std::shared_ptr<const ArgumentType>
integerArgument(std::int32_t min = std::numeric_limits<std::int32_t>::min(),
                std::int32_t max = std::numeric_limits<std::int32_t>::max());

[[nodiscard]] std::shared_ptr<const ArgumentType> angleArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> rotationArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> timeArgument(std::int32_t min = 0);

[[nodiscard]] std::shared_ptr<const ArgumentType> greedyStringArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> messageArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> gameProfileArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> gameModeArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> worldArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> playersArgument(bool single);

[[nodiscard]] std::shared_ptr<const ArgumentType> entitiesArgument(bool single = false);

[[nodiscard]] std::shared_ptr<const ArgumentType> optionalEntitiesArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> optionalPlayersArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> componentArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> resourceLocationArgument();

} // namespace basalt::server::runtime::command
