#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::command {

/// A block plus the properties written next to it, resolved to the state id that goes on the wire.
///
/// Only the property combinations this build's registry knows resolve. That is every default state
/// plus the non-default states worldgen writes; anything else is refused by name rather than
/// silently placed as the default, because a `/setblock` that quietly drops `facing=north` puts a
/// different block in the world than the one that was asked for.
struct BlockInput {
    wire::common::Identifier block;
    std::int32_t stateId = 0;
    std::map<std::string, std::string> properties;

    [[nodiscard]] bool isAir() const;

    friend bool operator==(const BlockInput&, const BlockInput&) = default;
};

/// What a tag resolves to, or nothing for a tag nobody declared.
using TagMembers = std::function<std::optional<std::set<wire::common::Identifier>>(const wire::common::Identifier&)>;

/// What `/fill ... replace <filter>` and `/execute if block` match against: a block, a `#tag`, and
/// in either case only the properties that were written, so `oak_log[axis=y]` matches a subset of
/// `oak_log`.
class BlockPredicateInput {
public:
    BlockPredicateInput(std::optional<wire::common::Identifier> block, std::optional<wire::common::Identifier> tag,
                        std::map<std::string, std::string> properties, TagMembers tagMembers);

    [[nodiscard]] bool matches(std::int32_t stateId) const;

private:
    std::optional<wire::common::Identifier> block_;
    std::optional<wire::common::Identifier> tag_;
    std::map<std::string, std::string> properties_;
    TagMembers tagMembers_;
};

/// `minecraft:item_predicate`: an item id or a `#tag` of them, as `/clear` filters with.
///
/// Components are not parsed: an item stack here carries only its id, so a predicate that named one
/// could never match anything.
class ItemPredicateInput {
public:
    ItemPredicateInput(std::optional<wire::common::Identifier> item, std::optional<wire::common::Identifier> tag,
                       TagMembers tagMembers);

    [[nodiscard]] bool matches(std::string_view itemId) const;

private:
    std::optional<wire::common::Identifier> item_;
    std::optional<wire::common::Identifier> tag_;
    TagMembers tagMembers_;
};

/// An item id with the components written after it, as `/give` and `/item` take one.
struct ParsedItem {
    wire::common::Identifier id;
    wire::nbt::NbtCompound components;
};

/// `minecraft:block_state`, as `/setblock`, `/fill` and `/clone` take one. The value is a
/// `BlockInput`.
[[nodiscard]] std::shared_ptr<const ArgumentType> blockStateArgument();

/// `minecraft:block_predicate`. The value is a `BlockPredicateInput`.
[[nodiscard]] std::shared_ptr<const ArgumentType> blockPredicateArgument();

/// `minecraft:item_predicate`. The value is an `ItemPredicateInput`.
[[nodiscard]] std::shared_ptr<const ArgumentType> itemPredicateArgument();

/// `minecraft:item_stack`: an item id and, optionally, `[component=value, ...]`. Every component the
/// registry knows is accepted, because every one of them is stored and persisted. The value is a
/// `ParsedItem`.
[[nodiscard]] std::shared_ptr<const ArgumentType> itemArgument();

/// `minecraft:score_holder`: a player selector, or a bare name for a holder that is not a player.
///
/// The bare-name form is the point: `#total`, `$counter` and any other invented name is a
/// legitimate score holder in vanilla, and a datapack that keeps counters relies on it. The value
/// is a `std::vector<std::string>`.
[[nodiscard]] std::shared_ptr<const ArgumentType> scoreHolderArgument();

/// `minecraft:operation`: what `/scoreboard players operation` does with the two scores. The value
/// is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> operationArgument();

/// `minecraft:nbt_compound_tag`: an SNBT compound, consumed exactly as far as it goes.
///
/// Declaring the real parser matters beyond parsing: the client validates the argument locally with
/// the same one, so a compound declared as a greedy string is drawn as an error even when the
/// server accepts it. The value is an `NbtCompound`.
[[nodiscard]] std::shared_ptr<const ArgumentType> nbtCompoundArgument();

/// `minecraft:nbt_tag`: any SNBT value, which is what `/data modify ... value` takes. The value is
/// an `NbtTag`.
[[nodiscard]] std::shared_ptr<const ArgumentType> nbtTagArgument();

/// `minecraft:nbt_path`: everything up to the next space, so `crew[{name:"bo"}].age` is one
/// argument. A quoted section may contain a space, which is why quotes are tracked rather than
/// splitting on the first one. The value is a `std::string`.
[[nodiscard]] std::shared_ptr<const ArgumentType> nbtPathArgument();

} // namespace basalt::server::runtime::command
