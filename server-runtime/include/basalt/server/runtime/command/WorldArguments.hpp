#pragma once

#include <basalt/server/runtime/command/CommandTree.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::command {

struct BlockInput {
    wire::common::Identifier block;
    std::int32_t stateId = 0;
    std::map<std::string, std::string> properties;

    [[nodiscard]] bool isAir() const;

    friend bool operator==(const BlockInput&, const BlockInput&) = default;
};

using TagMembers = std::function<std::optional<std::set<wire::common::Identifier>>(const wire::common::Identifier&)>;

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

struct ParsedItem {
    wire::common::Identifier id;
    wire::nbt::NbtCompound components;
};

[[nodiscard]] std::shared_ptr<const ArgumentType> blockStateArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> blockPredicateArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> itemPredicateArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> itemArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> scoreHolderArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> operationArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> nbtCompoundArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> nbtTagArgument();

[[nodiscard]] std::shared_ptr<const ArgumentType> nbtPathArgument();

} // namespace basalt::server::runtime::command
