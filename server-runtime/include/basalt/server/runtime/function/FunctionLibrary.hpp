#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::function {

/// One loaded function: its id and the command lines it runs, comments and blanks already dropped.
struct ServerFunction {
    wire::common::Identifier id;
    std::vector<std::string> lines;

    /// Which datapack it came from, so `/datapack disable` can take it away again.
    std::string pack;
};

/// The lines of a `.mcfunction` as vanilla reads them, or nothing when the file ends on a
/// continuation.
///
/// parity: `CommandFunction.fromLines`. Every line is trimmed, a line ending in a backslash is
/// joined to the one after it with the backslash dropped, and what is left is kept unless it is
/// empty or starts with `#`. A `$` line is a macro and is kept as written, since expanding it is
/// the executor's business.
[[nodiscard]] std::optional<std::vector<std::string>> parseFunction(std::string_view text);

/// Functions read from `datapacks/<pack>/data/<namespace>/function/<path>.mcfunction`.
///
/// The layout is vanilla's, so a datapack written for the real server drops in unchanged. Only
/// functions are read: recipes, loot tables and advancements in the same pack are ignored, and the
/// pack's own `pack.mcmeta` is read for nothing but its existence, which is what marks a directory
/// as a pack at all.
class FunctionLibrary {
public:
    /// Beside `worlds/` and `config/`, which is where an operator expects to drop a pack.
    static constexpr std::string_view kDatapacksRoot = "datapacks";

    /// Where `/datapack enable` puts a pack, which decides which of two packs wins a name.
    struct Position {
        enum class Kind : std::uint8_t { First, Last, Before, After };

        Kind kind = Kind::Last;
        std::string other;

        [[nodiscard]] static Position first() { return Position{.kind = Kind::First, .other = {}}; }

        [[nodiscard]] static Position last() { return Position{.kind = Kind::Last, .other = {}}; }

        [[nodiscard]] static Position before(std::string other) {
            return Position{.kind = Kind::Before, .other = std::move(other)};
        }

        [[nodiscard]] static Position after(std::string other) {
            return Position{.kind = Kind::After, .other = std::move(other)};
        }
    };

    explicit FunctionLibrary(std::string root = std::string{kDatapacksRoot},
                             wire::common::Logger logger = wire::common::Logger{"Functions"})
        : root_(std::move(root)), logger_(std::move(logger)) {}

    /// Every pack directory found, whether or not it is enabled.
    [[nodiscard]] const std::vector<std::string>& availablePacks() const noexcept { return availablePacks_; }

    [[nodiscard]] std::vector<std::string> enabledPacks() const;

    [[nodiscard]] const ServerFunction* get(const wire::common::Identifier& id) const;

    [[nodiscard]] std::vector<const ServerFunction*> all() const;

    [[nodiscard]] bool isEnabled(std::string_view pack) const;

    /// Answers whether this actually changed anything, so a command can say "already enabled".
    bool setEnabled(std::string_view pack, bool enabled, const Position& position = Position::last());

    /// Creates an empty pack directory with the `pack.mcmeta` that marks it as one.
    ///
    /// parity: `/datapack create`. Returns false if the name is already taken.
    bool create(const std::string& pack, const std::string& description);

    /// Reads every pack again. Called at startup and by `/reload`, exactly as vanilla reloads packs.
    std::size_t reload();

private:
    void place(std::string_view pack, const Position& position);
    void loadDirectory(const std::string& pack, const std::string& nameSpace, const std::string& directory,
                       const std::string& prefix);

    std::string root_;
    wire::common::Logger logger_;
    std::map<wire::common::Identifier, ServerFunction> functions_;
    std::set<std::string, std::less<>> disabledPacks_;

    /// Enabled packs in the order `/datapack enable ... before|after` put them.
    std::vector<std::string> order_;
    std::vector<std::string> availablePacks_;
};

} // namespace basalt::server::runtime::function
