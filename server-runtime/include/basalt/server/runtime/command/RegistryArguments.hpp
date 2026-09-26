#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace basalt::server::runtime::command {

/// A registry entry by name, declared the way vanilla declares it: same text as
/// `resourceLocationArgument`, but the client gets completions. `#tag` forms are refused, since
/// nothing behind these commands resolves a tag yet.

/// `minecraft:resource`: an entry that has to be loaded right now.
[[nodiscard]] std::shared_ptr<const ArgumentType> resourceArgument(std::string registry);

/// `minecraft:resource_key`: a name, whether or not anything is loaded under it.
[[nodiscard]] std::shared_ptr<const ArgumentType> resourceKeyArgument(std::string registry);

[[nodiscard]] std::shared_ptr<const ArgumentType> resourceOrTagArgument(std::string registry);

[[nodiscard]] std::shared_ptr<const ArgumentType> resourceOrTagKeyArgument(std::string registry);

/// `minecraft:resource_selector`: a name or a pattern matching several.
[[nodiscard]] std::shared_ptr<const ArgumentType> resourceSelectorArgument(std::string registry);

/// What `functionArgument` parses to: a function, or with `isTag` a function tag.
struct FunctionName {
    wire::common::Identifier id;
    bool isTag = false;
};

/// `minecraft:function`: a function or a `#tag` of them. The value is a `FunctionName`.
[[nodiscard]] std::shared_ptr<const ArgumentType> functionArgument();

/// `minecraft:dialog`: a dialog id, or an inline dialog the client parses itself.
[[nodiscard]] std::shared_ptr<const ArgumentType> dialogArgument();

/// `minecraft:objective`: the name of a scoreboard objective, which the client knows already. The
/// value is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> objectiveArgument();

/// `minecraft:objective_criteria`: what an objective counts. The value is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> objectiveCriteriaArgument();

/// `minecraft:team`: the name of a team, which the client also keeps. The value is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> teamArgument();

/// `minecraft:item_slot`: one inventory slot by its vanilla name. The value is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> itemSlotArgument();

/// `minecraft:int_range`: `3`, `3..`, `..7` or `3..7`, kept as written for the command to read. The
/// value is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> intRangeArgument();

/// `minecraft:hex_color`: `#rrggbb`, which is what a waypoint's own colour takes. The value is a
/// `std::int32_t`.
[[nodiscard]] std::shared_ptr<const ArgumentType> hexColorArgument();

/// `minecraft:team_color`: one of the sixteen chat colours a team can be painted with. The value is
/// a `wire::protocol::play::TeamColor`.
[[nodiscard]] std::shared_ptr<const ArgumentType> teamColorArgument();

/// `minecraft:scoreboard_slot`: where an objective is drawn. The value is a
/// `wire::protocol::play::DisplaySlot`.
[[nodiscard]] std::shared_ptr<const ArgumentType> scoreboardSlotArgument();

/// `minecraft:template_rotation`: how `/place template` turns what it places. The value is a
/// `server::world::structure::StructureRotation`.
[[nodiscard]] std::shared_ptr<const ArgumentType> templateRotationArgument();

/// `minecraft:template_mirror`: which way `/place template` flips what it places. The value is a
/// `server::world::structure::StructureMirror`.
[[nodiscard]] std::shared_ptr<const ArgumentType> templateMirrorArgument();

} // namespace basalt::server::runtime::command
