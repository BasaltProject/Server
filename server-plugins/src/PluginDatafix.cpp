#include <basalt/metamorph/datafix/DataFixer.hpp>
#include <basalt/metamorph/datafix/LevelData.hpp>
#include <basalt/server/plugins/PluginDatafix.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

namespace basalt::server::plugins::datafix {
namespace {

namespace fixer = metamorph::datafix;

[[nodiscard]] std::optional<fixer::Domain> domainOf(std::string_view type) {
    if (type == "chunk") {
        return fixer::Domain::Chunk;
    }
    if (type == "player") {
        return fixer::Domain::Player;
    }
    if (type == "entity_chunk") {
        return fixer::Domain::Entities;
    }
    return std::nullopt;
}

[[nodiscard]] bool isSavedData(std::string_view type) {
    return type == "random_sequences" || type == "raids" || type == "map" || type == "scoreboard";
}

} // namespace

bool supports(std::string_view type) { return domainOf(type).has_value() || isSavedData(type); }

std::optional<std::vector<std::uint8_t>> upgrade(std::string_view type, std::span<const std::uint8_t> nbt,
                                                 std::int32_t fromVersion, std::int32_t toVersion) {
    if (!supports(type)) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(nbt);
    const wire::nbt::NbtCompound* root = read.has_value() ? read->tag.get<wire::nbt::NbtCompound>() : nullptr;
    if (root == nullptr) {
        return std::nullopt;
    }
    wire::nbt::NbtCompound data = *root;
    if (const std::optional<fixer::Domain> domain = domainOf(type); domain.has_value()) {
        fixer::upgrade(*domain, data, fromVersion, {}, toVersion);
    } else {
        fixer::upgradeSavedData(type, data, fromVersion);
    }
    return wire::nbt::nbtIO::writeAnonymous(wire::nbt::NbtTag::ofCompound(std::move(data)));
}

} // namespace basalt::server::plugins::datafix
