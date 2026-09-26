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

/// A `$` line split around its `$(name)` holes. parity: `StringTemplate`.
struct MacroTemplate {
    std::vector<std::string> segments;
    /// Each hole, as an index into the function's parameters.
    std::vector<std::int32_t> variables;

    /// parity: `StringTemplate.substitute`, values given in parameter order.
    [[nodiscard]] std::string substitute(const std::vector<std::string>& values) const;
};

/// One line of a function: a command, or a macro to fill in first.
struct FunctionEntry {
    std::string command;
    std::optional<MacroTemplate> macro;
};

/// One loaded function. parity: `PlainTextFunction`, or `MacroFunction` once a `$` line appears.
struct ServerFunction {
    wire::common::Identifier id;
    std::vector<FunctionEntry> entries;
    /// The macro's parameters, in the order its lines first name them.
    std::vector<std::string> parameters;
    bool isMacro = false;
};

/// The lines of a `.mcfunction` as vanilla joins them: trimmed, a trailing backslash joining the
/// next line. Comments and blanks are kept out. Nothing when the file ends on a continuation.
[[nodiscard]] std::optional<std::vector<std::pair<std::int32_t, std::string>>> joinLines(std::string_view text);

/// Answers why a command line does not parse, or nothing when it does.
using CommandCheck = std::function<std::optional<std::string>(const std::string& line)>;

/// parity: `CommandFunction.fromLines`. `check` parses each plain line with the compilation source;
/// without one only the shape of the lines is checked.
[[nodiscard]] std::expected<ServerFunction, std::string>
compileFunction(const wire::common::Identifier& id, std::string_view text, const CommandCheck& check = {});

/// parity: `MacroFunction.instantiate`'s `stringify`: numbers without a trailing `.0`, strings bare,
/// everything else as SNBT.
[[nodiscard]] std::string macroArgument(const wire::nbt::NbtTag& value);

/// Functions and function tags read out of the selected data packs. parity: `ServerFunctionLibrary`.
class FunctionLibrary {
public:
    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    explicit FunctionLibrary(wire::common::Logger logger = wire::common::Logger{"Functions"})
        : logger_(std::move(logger)) {}

    /// Replaces everything with what the packs hold. A function that does not compile is logged and
    /// left out; a tag naming one that is missing is logged and left out too.
    std::size_t load(const EntriesUnder& entriesUnder, const ReadText& readText, const CommandCheck& check = {});

    [[nodiscard]] const ServerFunction* get(const wire::common::Identifier& id) const;

    /// parity: `getTag`, empty for a tag nobody declared.
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
