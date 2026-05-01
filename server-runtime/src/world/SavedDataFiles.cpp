#include <basalt/metamorph/datafix/DataFixer.hpp>
#include <basalt/server/runtime/world/SavedDataFiles.hpp>
#include <basalt/wire/common/logging/Logger.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <expected>
#include <string>
#include <vector>

namespace basalt::server::runtime::world::savedData {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

[[nodiscard]] std::optional<std::vector<std::uint8_t>> plain(std::vector<std::uint8_t> bytes) {
    if (bytes.size() > 2 && bytes[0] == 0x1F && bytes[1] == 0x8B) {
        auto unpacked = wire::io::gzip::decompress(bytes);
        return unpacked.has_value() ? std::optional{std::move(*unpacked)} : std::nullopt;
    }
    return bytes;
}

} // namespace

std::optional<NbtCompound> read(const std::string_view worldRoot, const std::string_view name) {
    const std::string own = std::string{worldRoot} + "/" + std::string{name} + ".nbt";
    const std::string vanilla = std::string{worldRoot} + "/data/minecraft/" + std::string{name} + ".dat";
    const bool imported = !wire::io::fileIO::exists(own);
    std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(imported ? vanilla : own);
    if (bytes.has_value()) {
        bytes = plain(std::move(*bytes));
    }
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> parsed = wire::nbt::nbtIO::read(*bytes);
    const NbtCompound* root = parsed.has_value() ? parsed->tag.get<NbtCompound>() : nullptr;
    const NbtCompound* data = root == nullptr ? nullptr : root->getCompound("data");
    if (data == nullptr) {
        wire::common::Logger{"world"}.warn(
            [&name] { return "The saved " + std::string{name} + " could not be read and is ignored."; });
        return std::nullopt;
    }
    const std::int32_t version = root->getInt("DataVersion", 0);
    if (imported && version != metamorph::datafix::kCurrentVersion) {
        wire::common::Logger{"world"}.warn([&name, version] {
            return "Imported " + std::string{name} + " from data version " + std::to_string(version) +
                   " and read as if it were the current one.";
        });
    }
    return *data;
}

bool write(const std::string_view worldRoot, const std::string_view name, NbtCompound data) {
    NbtCompound root;
    root.set("data", NbtTag::ofCompound(std::move(data)));
    root.putInt("DataVersion", metamorph::datafix::kCurrentVersion);
    return wire::io::fileIO::writeBytes(std::string{worldRoot} + "/" + std::string{name} + ".nbt",
                                        wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(root))));
}

} // namespace basalt::server::runtime::world::savedData
