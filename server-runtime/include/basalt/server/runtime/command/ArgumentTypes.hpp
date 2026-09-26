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

/// The argument types every command is built out of.
///
/// The two that read a selector (`minecraft:entity` and `minecraft:vec3`) arrive with
/// `EntitySelector`, which is what parses their grammar. Everything here reads its own text.

/// A single word, `brigadier:string` in its `SINGLE_WORD` form.
[[nodiscard]] std::shared_ptr<const ArgumentType> wordArgument();

/// `brigadier:string` in its `QUOTABLE_PHRASE` form: a word, or anything in double quotes.
[[nodiscard]] std::shared_ptr<const ArgumentType> stringArgument();

/// `brigadier:bool`, which accepts exactly `true` and `false`.
[[nodiscard]] std::shared_ptr<const ArgumentType> boolArgument();

/// `brigadier:double`, bounded the way the command that declares it needs.
[[nodiscard]] std::shared_ptr<const ArgumentType> doubleArgument(double min = -std::numeric_limits<double>::max(),
                                                                 double max = std::numeric_limits<double>::max());

/// `brigadier:float`, bounded the way the command that declares it needs.
[[nodiscard]] std::shared_ptr<const ArgumentType> floatArgument(float min = -std::numeric_limits<float>::max(),
                                                                float max = std::numeric_limits<float>::max());

/// `brigadier:integer`, bounded the way the command that declares it needs.
[[nodiscard]] std::shared_ptr<const ArgumentType>
integerArgument(std::int32_t min = std::numeric_limits<std::int32_t>::min(),
                std::int32_t max = std::numeric_limits<std::int32_t>::max());

/// `minecraft:angle`: one yaw in degrees, absolute or relative to whoever is being pointed.
[[nodiscard]] std::shared_ptr<const ArgumentType> angleArgument();

/// `minecraft:rotation`: a `yaw pitch` pair, each absolute or relative.
[[nodiscard]] std::shared_ptr<const ArgumentType> rotationArgument();

/// `minecraft:time`: a tick count, or a number with a unit (`10s`, `2d`, `40t`).
[[nodiscard]] std::shared_ptr<const ArgumentType> timeArgument(std::int32_t min = 0);

/// Everything left on the line, `brigadier:string` in its `GREEDY_PHRASE` form.
[[nodiscard]] std::shared_ptr<const ArgumentType> greedyStringArgument();

/// `minecraft:message`: the rest of the line, as chat text.
[[nodiscard]] std::shared_ptr<const ArgumentType> messageArgument();

/// `minecraft:game_profile`: a player name, which vanilla resolves against its profile cache so it
/// works on someone who is offline.
[[nodiscard]] std::shared_ptr<const ArgumentType> gameProfileArgument();

/// `minecraft:gamemode`. Accepts the four names vanilla's `GameType` serializes to, nothing else.
[[nodiscard]] std::shared_ptr<const ArgumentType> gameModeArgument();

/// A loaded world, by name.
[[nodiscard]] std::shared_ptr<const ArgumentType> worldArgument();

/// `minecraft:entity` restricted to players, vanilla's `EntityArgument.players`.
///
/// The same grammar as the entity form (`EntitySelector` parses it), so `@e[tag=admin]` here means
/// the players among them rather than a syntax error: vanilla's own `findPlayers` answers the same
/// selector the same way. The value is a `std::vector<player::PlayerSession*>`.
[[nodiscard]] std::shared_ptr<const ArgumentType> playersArgument(bool single);

/// `minecraft:entity`, over players *and* summoned entities. The value is a
/// `std::vector<CommandTarget>`.
[[nodiscard]] std::shared_ptr<const ArgumentType> entitiesArgument(bool single = false);

/// The same on the wire, but an empty match is an answer rather than an error, for the forms that
/// read `EntityArgument.getOptionalEntities`/`getOptionalPlayers`.
[[nodiscard]] std::shared_ptr<const ArgumentType> optionalEntitiesArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> optionalPlayersArgument();

/// `minecraft:component`: the rest of the line as a chat component in its JSON form, so styles and
/// events survive to the client. The value is a `wire::text::Component`.
[[nodiscard]] std::shared_ptr<const ArgumentType> componentArgument();

/// `minecraft:resource_location`: a namespaced id, checked for shape only. The value is a
/// `wire::common::Identifier`.
[[nodiscard]] std::shared_ptr<const ArgumentType> resourceLocationArgument();

} // namespace basalt::server::runtime::command
