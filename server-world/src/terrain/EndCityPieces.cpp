#include <basalt/server/world/terrain/EndCityPieces.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <utility>

namespace basalt::server::world::terrain::endCityPieces {
namespace {

using structure::StructureMirror;
using structure::StructurePos;
using structure::StructureRotation;

constexpr StructurePos kZero{0, 0, 0};
constexpr std::int32_t kMaxGenerationDepth = 8;

enum class Generator : std::uint8_t { HouseTower, Tower, TowerBridge, FatTower };

[[nodiscard]] StructureRotation rotatedBy(StructureRotation first, StructureRotation second) {
    static constexpr std::array<StructureRotation, 4> kOrder{StructureRotation::None, StructureRotation::Clockwise90,
                                                             StructureRotation::Clockwise180,
                                                             StructureRotation::Counterclockwise90};
    const auto ordinalOf = [](StructureRotation rotation) {
        for (std::size_t index = 0; index < kOrder.size(); ++index) {
            if (kOrder[index] == rotation) {
                return static_cast<std::int32_t>(index);
            }
        }
        return 0;
    };
    return kOrder[static_cast<std::size_t>((ordinalOf(first) + ordinalOf(second)) & 3)];
}

constexpr std::array<std::pair<StructureRotation, StructurePos>, 4> kTowerBridges{
    std::pair{StructureRotation::None, StructurePos{1, -1, 0}},
    std::pair{StructureRotation::Clockwise90, StructurePos{6, -1, 1}},
    std::pair{StructureRotation::Counterclockwise90, StructurePos{0, -1, 5}},
    std::pair{StructureRotation::Clockwise180, StructurePos{5, -1, 6}}};

constexpr std::array<std::pair<StructureRotation, StructurePos>, 4> kFatTowerBridges{
    std::pair{StructureRotation::None, StructurePos{4, -1, 0}},
    std::pair{StructureRotation::Clockwise90, StructurePos{12, -1, 4}},
    std::pair{StructureRotation::Counterclockwise90, StructurePos{0, -1, 8}},
    std::pair{StructureRotation::Clockwise180, StructurePos{8, -1, 12}}};

class Assembler {
public:
    Assembler(const structure::TemplateLoader& templateLoader, random::RandomSource& random)
        : templateLoader_(templateLoader), random_(random) {}

    [[nodiscard]] std::vector<Piece> assemble(StructurePos origin, StructureRotation rotation) {
        std::vector<Piece> pieces;
        std::size_t last = add(pieces, piece("base_floor", origin, rotation, true));
        last = add(pieces, child(pieces[last], StructurePos{-1, 0, -1}, "second_floor_1", rotation, false));
        last = add(pieces, child(pieces[last], StructurePos{-1, 4, -1}, "third_floor_1", rotation, false));
        last = add(pieces, child(pieces[last], StructurePos{-1, 8, -1}, "third_roof", rotation, true));
        recursiveChildren(Generator::Tower, 1, pieces[last], std::nullopt, pieces);
        return pieces;
    }

private:
    [[nodiscard]] bool generate(Generator generator, std::int32_t generationDepth, const Piece& parent,
                                const std::optional<StructurePos>& offset, std::vector<Piece>& pieces) {
        switch (generator) {
        case Generator::HouseTower:
            return generateHouseTower(generationDepth, parent, *offset, pieces);
        case Generator::Tower:
            return generateTower(generationDepth, parent, pieces);
        case Generator::TowerBridge:
            return generateTowerBridge(generationDepth, parent, pieces);
        case Generator::FatTower:
        default:
            return generateFatTower(generationDepth, parent, pieces);
        }
    }

    bool generateHouseTower(std::int32_t generationDepth, const Piece& parent, StructurePos offset,
                            std::vector<Piece>& pieces) {
        if (generationDepth > kMaxGenerationDepth) {
            return false;
        }
        const StructureRotation rotation = parent.rotation;
        std::size_t last = add(pieces, child(parent, offset, "base_floor", rotation, true));
        switch (random_.nextInt(3)) {
        case 0:
            add(pieces, child(pieces[last], StructurePos{-1, 4, -1}, "base_roof", rotation, true));
            break;
        case 1:
            last = add(pieces, child(pieces[last], StructurePos{-1, 0, -1}, "second_floor_2", rotation, false));
            last = add(pieces, child(pieces[last], StructurePos{-1, 8, -1}, "second_roof", rotation, false));
            recursiveChildren(Generator::Tower, generationDepth + 1, pieces[last], std::nullopt, pieces);
            break;
        case 2:
            last = add(pieces, child(pieces[last], StructurePos{-1, 0, -1}, "second_floor_2", rotation, false));
            last = add(pieces, child(pieces[last], StructurePos{-1, 4, -1}, "third_floor_2", rotation, false));
            last = add(pieces, child(pieces[last], StructurePos{-1, 8, -1}, "third_roof", rotation, true));
            recursiveChildren(Generator::Tower, generationDepth + 1, pieces[last], std::nullopt, pieces);
            break;
        default:
            break;
        }
        return true;
    }

    bool generateTower(std::int32_t generationDepth, const Piece& parent, std::vector<Piece>& pieces) {
        const StructureRotation rotation = parent.rotation;
        std::size_t last = add(pieces, child(parent, StructurePos{3 + random_.nextInt(2), -3, 3 + random_.nextInt(2)},
                                             "tower_base", rotation, true));
        last = add(pieces, child(pieces[last], StructurePos{0, 7, 0}, "tower_piece", rotation, true));
        std::optional<std::size_t> bridgePiece;
        if (random_.nextInt(3) == 0) {
            bridgePiece = last;
        }
        const std::int32_t towerHeight = 1 + random_.nextInt(3);
        for (std::int32_t index = 0; index < towerHeight; ++index) {
            last = add(pieces, child(pieces[last], StructurePos{0, 4, 0}, "tower_piece", rotation, true));
            if (index < towerHeight - 1 && random_.nextBoolean()) {
                bridgePiece = last;
            }
        }

        if (bridgePiece.has_value()) {
            for (const auto& [bridgeRotation, bridgeOffset] : kTowerBridges) {
                if (!random_.nextBoolean()) {
                    continue;
                }
                const std::size_t bridgeStart = add(pieces, child(pieces[*bridgePiece], bridgeOffset, "bridge_end",
                                                                  rotatedBy(rotation, bridgeRotation), true));
                recursiveChildren(Generator::TowerBridge, generationDepth + 1, pieces[bridgeStart], std::nullopt,
                                  pieces);
            }
            add(pieces, child(pieces[last], StructurePos{-1, 4, -1}, "tower_top", rotation, true));
        } else {
            if (generationDepth != 7) {
                return recursiveChildren(Generator::FatTower, generationDepth + 1, pieces[last], std::nullopt, pieces);
            }
            add(pieces, child(pieces[last], StructurePos{-1, 4, -1}, "tower_top", rotation, true));
        }
        return true;
    }

    bool generateTowerBridge(std::int32_t generationDepth, const Piece& parent, std::vector<Piece>& pieces) {
        const StructureRotation rotation = parent.rotation;
        const std::int32_t bridgeLength = random_.nextInt(4) + 1;
        std::size_t last = add(pieces, child(parent, StructurePos{0, 0, -4}, "bridge_piece", rotation, true));
        pieces[last].generationDepth = -1;
        std::int32_t nextY = 0;
        for (std::int32_t index = 0; index < bridgeLength; ++index) {
            if (random_.nextBoolean()) {
                last = add(pieces, child(pieces[last], StructurePos{0, nextY, -4}, "bridge_piece", rotation, true));
                nextY = 0;
            } else {
                if (random_.nextBoolean()) {
                    last = add(pieces,
                               child(pieces[last], StructurePos{0, nextY, -4}, "bridge_steep_stairs", rotation, true));
                } else {
                    last = add(pieces,
                               child(pieces[last], StructurePos{0, nextY, -8}, "bridge_gentle_stairs", rotation, true));
                }
                nextY = 4;
            }
        }

        if (!shipCreated_ && random_.nextInt(10 - generationDepth) == 0) {
            add(pieces, child(pieces[last], StructurePos{-8 + random_.nextInt(8), nextY, -70 + random_.nextInt(10)},
                              "ship", rotation, true));
            shipCreated_ = true;
        } else if (!recursiveChildren(Generator::HouseTower, generationDepth + 1, pieces[last],
                                      StructurePos{-3, nextY + 1, -11}, pieces)) {
            return false;
        }

        last = add(pieces, child(pieces[last], StructurePos{4, nextY, 0}, "bridge_end",
                                 rotatedBy(rotation, StructureRotation::Clockwise180), true));
        pieces[last].generationDepth = -1;
        return true;
    }

    bool generateFatTower(std::int32_t generationDepth, const Piece& parent, std::vector<Piece>& pieces) {
        const StructureRotation rotation = parent.rotation;
        std::size_t last = add(pieces, child(parent, StructurePos{-3, 4, -3}, "fat_tower_base", rotation, true));
        last = add(pieces, child(pieces[last], StructurePos{0, 4, 0}, "fat_tower_middle", rotation, true));
        std::int32_t index = 0;
        while (index < 2 && random_.nextInt(3) != 0) {
            last = add(pieces, child(pieces[last], StructurePos{0, 8, 0}, "fat_tower_middle", rotation, true));
            for (const auto& [bridgeRotation, bridgeOffset] : kFatTowerBridges) {
                if (!random_.nextBoolean()) {
                    continue;
                }
                const std::size_t bridgeStart = add(
                    pieces, child(pieces[last], bridgeOffset, "bridge_end", rotatedBy(rotation, bridgeRotation), true));
                recursiveChildren(Generator::TowerBridge, generationDepth + 1, pieces[bridgeStart], std::nullopt,
                                  pieces);
            }
            ++index;
        }
        add(pieces, child(pieces[last], StructurePos{-2, 8, -2}, "fat_tower_top", rotation, true));
        return true;
    }

    /// The parent is taken by value: it usually lives in `pieces`, which this very call then grows.
    bool recursiveChildren(Generator generator, std::int32_t generationDepth, Piece parent,
                           const std::optional<StructurePos>& offset, std::vector<Piece>& pieces) {
        if (generationDepth > kMaxGenerationDepth) {
            return false;
        }
        const std::int32_t parentDepth = parent.generationDepth;
        std::vector<Piece> childPieces;
        if (!generate(generator, generationDepth, parent, offset, childPieces)) {
            return false;
        }
        const std::int32_t childTag = random_.nextInt();
        for (Piece& child : childPieces) {
            child.generationDepth = childTag;
            const auto collision = std::find_if(pieces.begin(), pieces.end(), [&child](const Piece& placed) {
                return placed.boundingBox.intersects(child.boundingBox);
            });
            if (collision != pieces.end() && collision->generationDepth != parentDepth) {
                return false;
            }
        }
        pieces.insert(pieces.end(), childPieces.begin(), childPieces.end());
        return true;
    }

    [[nodiscard]] Piece child(const Piece& parent, StructurePos offset, std::string_view templateName,
                              StructureRotation rotation, bool overwrite) const {
        const StructurePos relative = structure::transform(offset, StructureMirror::None, parent.rotation, kZero);
        return piece(templateName,
                     StructurePos{parent.position.x + relative.x, parent.position.y + relative.y,
                                  parent.position.z + relative.z},
                     rotation, overwrite);
    }

    [[nodiscard]] Piece piece(std::string_view templateName, StructurePos position, StructureRotation rotation,
                              bool overwrite) const {
        const structure::StructureTemplate* found =
            templateLoader_(wire::common::Identifier::vanilla("end_city/" + std::string{templateName}));
        Piece built{.templateName = std::string{templateName},
                    .position = position,
                    .rotation = rotation,
                    .overwrite = overwrite,
                    .piece = found};
        if (found != nullptr) {
            built.boundingBox = found->getBoundingBox(position, rotation, kZero, StructureMirror::None);
        }
        return built;
    }

    static std::size_t add(std::vector<Piece>& pieces, Piece piece) {
        pieces.push_back(std::move(piece));
        return pieces.size() - 1;
    }

    const structure::TemplateLoader& templateLoader_;
    random::RandomSource& random_;
    bool shipCreated_ = false;
};

} // namespace

std::vector<Piece> assemble(const structure::TemplateLoader& templateLoader, StructurePos origin,
                            StructureRotation rotation, random::RandomSource& random) {
    return Assembler{templateLoader, random}.assemble(origin, rotation);
}

} // namespace basalt::server::world::terrain::endCityPieces
