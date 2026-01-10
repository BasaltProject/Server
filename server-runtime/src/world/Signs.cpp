#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Signs.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world::signs {
namespace {

constexpr std::string_view kFront = "front_text";
constexpr std::string_view kBack = "back_text";

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* id = registry::blockStateDefinitions::blockOf(stateId);
    return id == nullptr ? std::string{} : id->path();
}

/// parity: `SignText`'s codec: the four messages, a colour and the glow flag, always present.
[[nodiscard]] wire::nbt::NbtCompound side(const std::vector<std::string>& lines) {
    wire::nbt::NbtList messages{wire::nbt::NbtType::String};
    for (std::int32_t line = 0; line < kLines; ++line) {
        const auto index = static_cast<std::size_t>(line);
        messages.push(wire::nbt::NbtTag::ofString(index < lines.size() ? lines[index] : std::string{}));
    }
    wire::nbt::NbtCompound text;
    text.set("messages", wire::nbt::NbtTag::ofList(std::move(messages)));
    text.putString("color", "black");
    text.putByte("has_glowing_text", 0);
    return text;
}

} // namespace

bool isSign(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    return path.ends_with("_sign") || path.ends_with("_hanging_sign");
}

std::optional<wire::common::Identifier> typeOf(std::int32_t stateId) {
    const std::string path = pathOf(stateId);
    if (path.ends_with("_hanging_sign")) {
        return wire::common::Identifier::vanilla("hanging_sign");
    }
    if (path.ends_with("_sign")) {
        return wire::common::Identifier::vanilla("sign");
    }
    return std::nullopt;
}

wire::nbt::NbtCompound emptyData() {
    wire::nbt::NbtCompound data;
    data.set(kFront, wire::nbt::NbtTag::ofCompound(side({})));
    data.set(kBack, wire::nbt::NbtTag::ofCompound(side({})));
    data.putByte("is_waxed", 0);
    return data;
}

wire::nbt::NbtCompound withText(const wire::nbt::NbtCompound& data, bool isFront,
                                const std::vector<std::string>& lines) {
    std::vector<std::string> trimmed;
    trimmed.reserve(lines.size());
    for (const std::string& line : lines) {
        trimmed.push_back(line.substr(0, kMaxLineLength));
    }
    wire::nbt::NbtCompound updated = data;
    updated.set(isFront ? kFront : kBack, wire::nbt::NbtTag::ofCompound(side(trimmed)));
    return updated;
}

bool isWaxed(const wire::nbt::NbtCompound& data) { return data.getByte("is_waxed") != 0; }

server::world::ChunkBlockEntity blockEntity(std::int32_t localX, std::int32_t y, std::int32_t localZ,
                                            const wire::common::Identifier& type, wire::nbt::NbtCompound data) {
    return server::world::ChunkBlockEntity{localX, y, localZ, type, std::move(data)};
}

} // namespace basalt::server::runtime::world::signs
