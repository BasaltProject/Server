#include <basalt/server/runtime/world/RealDialogs.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <algorithm>
#include <expected>

namespace basalt::server::runtime::world {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;

constexpr std::string_view kPrefix = "data/minecraft/dialog/";
constexpr std::string_view kJsonSuffix = ".json";

[[nodiscard]] std::string stripped(const std::string& entry) {
    std::string path = entry;
    if (path.starts_with(kPrefix)) {
        path.erase(0, kPrefix.size());
    }
    if (path.ends_with(kJsonSuffix)) {
        path.resize(path.size() - kJsonSuffix.size());
    }
    return path;
}

/// One entry as the registry packet wants it, or why it cannot travel as NBT.
[[nodiscard]] std::expected<NbtCompound, std::string> convert(const std::optional<std::string>& text) {
    if (!text.has_value()) {
        return std::unexpected(std::string{"no such entry"});
    }
    const std::expected<wire::common::JsonValue, wire::common::JsonError> json = wire::common::parseJson(*text);
    if (!json.has_value()) {
        return std::unexpected(std::string{"malformed JSON"});
    }
    const std::expected<wire::nbt::NbtTag, wire::nbt::JsonToNbtError> tag = wire::nbt::jsonToNbt(*json);
    if (!tag.has_value()) {
        return std::unexpected(std::string{tag.error() == wire::nbt::JsonToNbtError::NullHasNoEquivalent
                                               ? "a null has no NBT equivalent"
                                               : "a list mixes types"});
    }
    const NbtCompound* compound = tag->get<NbtCompound>();
    if (compound == nullptr) {
        return std::unexpected(std::string{"not a compound"});
    }
    return *compound;
}

} // namespace

RealDialogs::RealDialogs(const Jar& jar, wire::common::Logger logger) : logger_(std::move(logger)) {
    if (!jar.entriesUnder || !jar.readText) {
        return;
    }
    std::vector<std::string> names = jar.entriesUnder(kPrefix);
    std::sort(names.begin(), names.end());
    for (const std::string& entry : names) {
        if (!entry.ends_with(kJsonSuffix)) {
            continue;
        }
        const std::string path = stripped(entry);
        std::expected<NbtCompound, std::string> converted = convert(jar.readText(entry));
        if (!converted.has_value()) {
            logger_.warn([&] {
                return "Dialog '" + path + "' could not be converted (" + converted.error() +
                       "); sending no dialog registry, because a partial one would disconnect clients at login.";
            });
            entries_.clear();
            indices_.clear();
            return;
        }
        indices_.emplace(Identifier::vanilla(path), static_cast<std::int32_t>(entries_.size()));
        entries_.emplace_back(Identifier::vanilla(path), std::move(*converted));
    }
    if (!entries_.empty()) {
        logger_.info([&] { return "Loaded " + std::to_string(entries_.size()) + " dialog(s)."; });
    }
}

std::vector<Identifier> RealDialogs::ids() const {
    std::vector<Identifier> ids;
    ids.reserve(entries_.size());
    for (const auto& [id, definition] : entries_) {
        ids.push_back(id);
    }
    return ids;
}

std::optional<std::int32_t> RealDialogs::protocolIdOf(const Identifier& id) const {
    const auto found = indices_.find(id);
    return found == indices_.end() ? std::nullopt : std::optional{found->second};
}

bool RealDialogs::contains(const Identifier& id) const { return indices_.contains(id); }

} // namespace basalt::server::runtime::world
