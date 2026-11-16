#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/VanillaTables.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace basalt::server::registry::blockShapes {
namespace {
constexpr std::string_view kIndexAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

struct Outline {
    int offsetKind = 0;
    float maxHorizontal = 0.0F;
    float maxVertical = 0.0F;
    std::vector<double> boxes;
    std::vector<double> visual;
};

struct OutlineTable {
    std::vector<Outline> outlines;
    std::vector<std::int32_t> byState;
};

[[nodiscard]] double parseDecimal(std::string_view text) {
    double value = 0.0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

[[nodiscard]] std::vector<std::string_view> splitOn(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    while (true) {
        const std::size_t at = text.find(separator);
        if (at == std::string_view::npos) {
            parts.push_back(text);
            return parts;
        }
        parts.push_back(text.substr(0, at));
        text = text.substr(at + 1);
    }
}

[[nodiscard]] const OutlineTable& outlineTable() {
    static const OutlineTable built = [] {
        OutlineTable table;
        for (const std::string_view encoded : blockOutlineSpecsTable()) {
            const std::vector<std::string_view> parts = splitOn(encoded, '|');
            Outline outline;
            outline.offsetKind = static_cast<int>(parseDecimal(parts[0]));
            outline.maxHorizontal = static_cast<float>(parseDecimal(parts[1]));
            outline.maxVertical = static_cast<float>(parseDecimal(parts[2]));
            const auto fill = [](std::string_view text, std::vector<double>& into) {
                for (const std::string_view box : splitOn(text, ';')) {
                    if (box.empty()) {
                        continue;
                    }
                    for (const std::string_view corner : splitOn(box, ',')) {
                        into.push_back(parseDecimal(corner));
                    }
                }
            };
            fill(parts[3], outline.boxes);
            fill(parts[4], outline.visual);
            table.outlines.push_back(std::move(outline));
        }
        const std::string_view index = blockOutlineIndexTable();
        table.byState.reserve(index.size() / 2);
        for (std::size_t state = 0; state * 2 + 1 < index.size(); ++state) {
            const auto high = static_cast<std::int32_t>(kIndexAlphabet.find(index[state * 2]));
            const auto low = static_cast<std::int32_t>(kIndexAlphabet.find(index[state * 2 + 1]));
            table.byState.push_back(high * static_cast<std::int32_t>(kIndexAlphabet.size()) + low);
        }
        return table;
    }();
    return built;
}

// parity: `Mth.getSeed(x, 0, z)`.
[[nodiscard]] std::int64_t offsetSeed(std::int32_t x, std::int32_t z) {
    const std::int64_t mixed =
        static_cast<std::int64_t>(static_cast<std::int32_t>(static_cast<std::uint32_t>(x) * 3129871U)) ^
        (static_cast<std::int64_t>(z) * 116129781LL);
    const auto seed =
        static_cast<std::int64_t>(static_cast<std::uint64_t>(mixed) * static_cast<std::uint64_t>(mixed) * 42317861ULL +
                                  static_cast<std::uint64_t>(mixed) * 11ULL);
    return seed >> 16;
}

[[nodiscard]] double clampOffset(double value, float bound) {
    return std::clamp(value, -static_cast<double>(bound), static_cast<double>(bound));
}
} // namespace

// parity: `BlockState.getShape` with the `offsetType` of the block applied at the column.
std::vector<double> outlineBoxes(std::int32_t stateId, std::int32_t x, std::int32_t z) {
    const OutlineTable& table = outlineTable();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= table.byState.size()) {
        return {};
    }
    const Outline& outline = table.outlines[static_cast<std::size_t>(table.byState[static_cast<std::size_t>(stateId)])];
    std::vector<double> boxes = outline.boxes;
    if (outline.offsetKind == 0) {
        return boxes;
    }
    const std::int64_t seed = offsetSeed(x, z);
    const double offsetX =
        clampOffset((static_cast<double>(static_cast<float>(seed & 15) / 15.0F) - 0.5) * 0.5, outline.maxHorizontal);
    const double offsetZ = clampOffset((static_cast<double>(static_cast<float>((seed >> 8) & 15) / 15.0F) - 0.5) * 0.5,
                                       outline.maxHorizontal);
    const double offsetY = outline.offsetKind == 2
                               ? (static_cast<double>(static_cast<float>((seed >> 4) & 15) / 15.0F) - 1.0) *
                                     static_cast<double>(outline.maxVertical)
                               : 0.0;
    for (std::size_t corner = 0; corner + 2 < boxes.size(); corner += 3) {
        boxes[corner] += offsetX;
        boxes[corner + 1] += offsetY;
        boxes[corner + 2] += offsetZ;
    }
    return boxes;
}

// parity: `BlockState.getVisualShape`, which is the collision shape unless a block says otherwise.
std::vector<double> visualBoxes(std::int32_t stateId) {
    const OutlineTable& table = outlineTable();
    if (stateId < 0 || static_cast<std::size_t>(stateId) >= table.byState.size()) {
        return {};
    }
    return table.outlines[static_cast<std::size_t>(table.byState[static_cast<std::size_t>(stateId)])].visual;
}
} // namespace basalt::server::registry::blockShapes
