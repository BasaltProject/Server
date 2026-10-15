#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace basalt::server::runtime::command {

[[nodiscard]] std::shared_ptr<const ArgumentType> resourceArgument(std::string registry);

[[nodiscard]] std::shared_ptr<const ArgumentType> resourceKeyArgument(std::string registry);

[[nodiscard]] std::shared_ptr<const ArgumentType> resourceOrTagArgument(std::string registry);

[[nodiscard]] std::shared_ptr<const ArgumentType> resourceOrTagKeyArgument(std::string registry);

[[nodiscard]] std::shared_ptr<const ArgumentType> resourceSelectorArgument(std::string registry);

struct FunctionName {
    wire::common::Identifier id;
    bool isTag = false;
};

[[nodiscard]] std::shared_ptr<const ArgumentType> functionArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> dialogArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> objectiveArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> objectiveCriteriaArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> teamArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> itemSlotArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> intRangeArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> hexColorArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> teamColorArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> scoreboardSlotArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> templateRotationArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> templateMirrorArgument();

} // namespace basalt::server::runtime::command
