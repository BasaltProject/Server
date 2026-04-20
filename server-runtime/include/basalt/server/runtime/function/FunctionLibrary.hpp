#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::nbt {
class NbtTag;
}

namespace basalt::server::runtime::function {

struct MacroTemplate {
    std::vector<std::string> segments;
    std::vector<std::int32_t> variables;

    [[nodiscard]] std::string substitute(const std::vector<std::string>& values) const;
};

struct FunctionEntry {
    std::string command;
    std::optional<MacroTemplate> macro;
};

struct ServerFunction {
    wire::common::Identifier id;
    std::vector<FunctionEntry> entries;
    std::vector<std::string> parameters;
    bool isMacro = false;
};

[[nodiscard]] std::optional<std::vector<std::pair<std::int32_t, std::string>>> joinLines(std::string_view text);

using CommandCheck = std::function<std::optional<std::string>(const std::string& line)>;

[[nodiscard]] std::expected<ServerFunction, std::string>
compileFunction(const wire::common::Identifier& id, std::string_view text, const CommandCheck& check = {});

/// parity: `MacroFunction.instantiate`'s `stringify`.
[[nodiscard]] std::string macroArgument(const wire::nbt::NbtTag& value);

class FunctionLibrary {
public:
    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    explicit FunctionLibrary(wire::common::Logger logger = wire::common::Logger{"Functions"})
        : logger_(std::move(logger)) {}

    std::size_t load(const EntriesUnder& entriesUnder, const ReadText& readText, const CommandCheck& check = {});

    [[nodiscard]] const ServerFunction* get(const wire::common::Identifier& id) const;

    [[nodiscard]] std::vector<const ServerFunction*> tag(const wire::common::Identifier& id) const;

    [[nodiscard]] bool hasTag(const wire::common::Identifier& id) const { return tags_.contains(id); }

    [[nodiscard]] std::vector<const ServerFunction*> all() const;

    [[nodiscard]] std::vector<wire::common::Identifier> functionIds() const;

    [[nodiscard]] std::vector<wire::common::Identifier> tagIds() const;

private:
    wire::common::Logger logger_;
    std::map<wire::common::Identifier, ServerFunction> functions_;
    std::map<wire::common::Identifier, std::vector<wire::common::Identifier>> tags_;
};

} // namespace basalt::server::runtime::function
