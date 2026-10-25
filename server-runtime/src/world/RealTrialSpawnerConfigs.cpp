#include <basalt/server/runtime/datapack/DataFiles.hpp>
#include <basalt/server/runtime/world/RealTrialSpawnerConfigs.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <expected>

namespace basalt::server::runtime::world {

RealTrialSpawnerConfigs::RealTrialSpawnerConfigs(const Jar& jar, wire::common::Logger logger)
    : logger_(std::move(logger)) {
    if (!jar.entriesUnder || !jar.readText) {
        return;
    }
    for (const auto& [id, entry] : datapack::dataFiles(jar.entriesUnder, "trial_spawner")) {
        const std::optional<std::string> text = jar.readText(entry);
        const std::expected<wire::common::JsonValue, wire::common::JsonError> json =
            text.has_value() ? wire::common::parseJson(*text) : std::unexpected(wire::common::JsonError{});
        const std::expected<wire::nbt::NbtTag, wire::nbt::JsonToNbtError> tag =
            json.has_value() ? wire::nbt::jsonToNbt(*json) : std::unexpected(wire::nbt::JsonToNbtError{});
        const wire::nbt::NbtCompound* compound = tag.has_value() ? tag->get<wire::nbt::NbtCompound>() : nullptr;
        if (compound == nullptr) {
            logger_.warn([&] { return "Trial spawner config '" + id.toString() + "' could not be read."; });
            continue;
        }
        entries_.emplace_back(id, *compound);
    }
    if (!entries_.empty()) {
        logger_.info([&] { return "Loaded " + std::to_string(entries_.size()) + " trial spawner config(s)."; });
    }
}

const wire::nbt::NbtCompound* RealTrialSpawnerConfigs::definitionOf(const wire::common::Identifier& id) const {
    for (const auto& [name, definition] : entries_) {
        if (name == id) {
            return &definition;
        }
    }
    return nullptr;
}

} // namespace basalt::server::runtime::world
