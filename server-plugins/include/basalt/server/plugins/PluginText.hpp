#pragma once

#include <basalt/server/lua/LuaArgs.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::plugins {

/// What a plugin writes, before it reaches anyone: `@key` looked up in its own `lang/`, and `{name}`
/// filled in from a table.
///
/// Both are the same idea: a plugin writes what it means and never assembles a string, so a
/// translated line and a line with a number in it are written the same way.
class PluginText {
public:
    PluginText(std::string folder, std::string defaultLocale)
        : folder_(std::move(folder)), defaultLocale_(std::move(defaultLocale)) {}

    /// Reads every `lang/<locale>.toml` the plugin has. Missing is not an error: most have none.
    void load();

    /// Resolves one message: `@key` becomes its translation, `{name}` becomes its argument.
    ///
    /// A key with no translation comes back as itself rather than as an empty line, because a
    /// missing string should look like a missing string to whoever has to fix it. `arguments` is the
    /// stack index of the table to fill from, or zero for none.
    [[nodiscard]] std::string resolve(std::string_view message, lua::LuaArgs* lua, std::int32_t arguments) const;

    /// The translation of one key, or nothing when no locale has it.
    [[nodiscard]] std::optional<std::string> translation(std::string_view key) const;

    /// How many locales were read, for `/plugins` and for a test that has to see it worked.
    [[nodiscard]] std::size_t localeCount() const noexcept { return locales_.size(); }

private:
    std::string folder_;
    std::string defaultLocale_;
    std::map<std::string, std::map<std::string, std::string, std::less<>>, std::less<>> locales_;
};

} // namespace basalt::server::plugins
