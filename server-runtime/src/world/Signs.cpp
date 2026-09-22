#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/Signs.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <string>
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

std::string plainText(const wire::nbt::NbtCompound& data, const bool front) {
    const wire::nbt::NbtCompound* side = data.getCompound(front ? kFront : kBack);
    if (side == nullptr) {
        return {};
    }
    const wire::nbt::NbtList* messages = side->getList("messages");
    if (messages == nullptr) {
        return {};
    }
    std::string text;
    for (const wire::nbt::NbtTag& line : *messages) {
        if (!text.empty()) {
            text += '\n';
        }
        const std::string* raw = line.get<std::string>();
        if (raw == nullptr) {
            continue;
        }
        const std::expected<wire::common::JsonValue, wire::common::JsonError> parsed = wire::common::parseJson(*raw);
        text += parsed.has_value() ? wire::text::toPlainText(wire::text::componentJson::deserialize(*parsed)) : *raw;
    }
    return text;
}

bool isWaxed(const wire::nbt::NbtCompound& data) { return data.getByte("is_waxed") != 0; }

wire::nbt::NbtCompound withComponents(const wire::nbt::NbtCompound& data, const wire::nbt::NbtCompound& components) {
    wire::nbt::NbtCompound updated = data;
    if (const wire::nbt::NbtCompound* front = components.getCompound("minecraft:sign_text_front"); front != nullptr) {
        updated.set(kFront, wire::nbt::NbtTag::ofCompound(*front));
    }
    if (const wire::nbt::NbtCompound* back = components.getCompound("minecraft:sign_text_back"); back != nullptr) {
        updated.set(kBack, wire::nbt::NbtTag::ofCompound(*back));
    }
    if (components.contains("minecraft:waxed")) {
        updated.putByte("is_waxed", 1);
    }
    return updated;
}

bool hasEditableText(const wire::nbt::NbtCompound& data, const bool front) {
    const wire::nbt::NbtCompound* side = data.getCompound(front ? kFront : kBack);
    const wire::nbt::NbtList* messages = side == nullptr ? nullptr : side->getList("messages");
    if (messages == nullptr) {
        return true;
    }
    // parity: `PlainTextContents`: a bare string, or a compound whose content is its `text`.
    static constexpr std::array<std::string_view, 7> kOtherContents{"translate", "score",  "selector", "keybind",
                                                                    "nbt",       "object", "sprite"};
    return std::ranges::all_of(*messages, [](const wire::nbt::NbtTag& line) {
        const wire::nbt::NbtCompound* component = line.get<wire::nbt::NbtCompound>();
        return component == nullptr || std::ranges::none_of(kOtherContents, [component](std::string_view key) {
                   return component->contains(key);
               });
    });
}

bool facesFront(const std::int32_t stateId, const std::int32_t x, const std::int32_t z, const double playerX,
                const double playerZ) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const std::string path = pathOf(stateId);
    double centreX = 0.5;
    double centreZ = 0.5;
    float signYaw = 0.0F;
    if (const auto rotation = properties.find("rotation"); rotation != properties.end()) {
        // parity: `RotationSegment.convertToDegrees`, sixteen to the turn.
        signYaw = static_cast<float>(std::stoi(rotation->second)) * 22.5F;
    } else if (const auto facing = properties.find("facing"); facing != properties.end()) {
        // parity: `Direction.toYRot`: south, west, north, east.
        static constexpr std::array<std::string_view, 4> kQuarters{"south", "west", "north", "east"};
        const auto quarter = std::ranges::find(kQuarters, facing->second);
        signYaw = static_cast<float>(quarter - kQuarters.begin()) * 90.0F;
        // parity: `WallSignBlock.getSignHitboxCenterPosition`, the middle of the board against its wall.
        if (!path.ends_with("_hanging_sign")) {
            constexpr double kNear = 0.0625;
            constexpr double kFar = 0.9375;
            centreZ = facing->second == "north" ? kFar : facing->second == "south" ? kNear : 0.5;
            centreX = facing->second == "west" ? kFar : facing->second == "east" ? kNear : 0.5;
        }
    }
    const double dx = playerX - (x + centreX);
    const double dz = playerZ - (z + centreZ);
    const auto playerYaw =
        static_cast<float>(std::atan2(dz, dx) * static_cast<double>(180.0F / std::numbers::pi_v<float>)) - 90.0F;
    // parity: `Mth.degreesDifferenceAbs` over `wrapDegrees`.
    float difference = std::fmod(playerYaw - signYaw, 360.0F);
    if (difference >= 180.0F) {
        difference -= 360.0F;
    }
    if (difference < -180.0F) {
        difference += 360.0F;
    }
    return std::abs(difference) <= 90.0F;
}

server::world::ChunkBlockEntity blockEntity(std::int32_t localX, std::int32_t y, std::int32_t localZ,
                                            const wire::common::Identifier& type, wire::nbt::NbtCompound data) {
    return server::world::ChunkBlockEntity{localX, y, localZ, type, std::move(data)};
}

} // namespace basalt::server::runtime::world::signs
