#include <basalt/server/random/RandomSupport.hpp>
#include <basalt/server/runtime/world/RandomSequences.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {
namespace {

constexpr std::size_t kSeedLongs = 2;

[[nodiscard]] std::string pathFor(std::string_view worldRoot) {
    return std::string{worldRoot} + "/random_sequences.nbt";
}

} // namespace

random::RandomSource& RandomSequences::of(const wire::common::Identifier& name) {
    const std::lock_guard<std::mutex> held{mutex_};
    dirty_ = true;
    const auto found = streams_.find(name);
    if (found != streams_.end()) {
        return found->second;
    }
    // parity: `RandomSequence.createSequence`: the world seed upgraded but **not** mixed, xored with
    // the md5 of the name, and only then mixed. The salt is zero and both flags are on, which is
    // what a world that nobody has touched with `/random reset` carries.
    const random::Seed128bit seed =
        random::upgradeSeedTo128bitUnmixed(worldSeed_).xorWith(random::seedFromHashOf(name.toString())).mixed();
    return streams_.emplace(name, random::RandomSource::xoroshiro(seed)).first->second;
}

void RandomSequences::load(const std::string_view worldRoot) {
    const std::lock_guard<std::mutex> held{mutex_};
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(pathFor(worldRoot));
    if (!bytes.has_value()) {
        return;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*bytes);
    if (!read.has_value()) {
        return;
    }
    const wire::nbt::NbtCompound* root = read->tag.get<wire::nbt::NbtCompound>();
    if (root == nullptr) {
        return;
    }
    for (const std::string& name : root->names()) {
        const std::span<const std::int64_t> seed = root->getLongArray(name);
        if (seed.size() != kSeedLongs) {
            continue;
        }
        const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
            wire::common::Identifier::parse(name);
        if (!id.has_value()) {
            continue;
        }
        streams_.insert_or_assign(*id, random::RandomSource::xoroshiro(seed[0], seed[1]));
    }
    dirty_ = false;
}

void RandomSequences::save(const std::string_view worldRoot) {
    const std::lock_guard<std::mutex> held{mutex_};
    if (!dirty_) {
        return;
    }
    wire::nbt::NbtCompound root;
    for (const auto& [name, stream] : streams_) {
        // parity: `Xoroshiro128PlusPlus.CODEC`, the two longs of the generator and nothing more. A
        // gaussian drawn half way is not written down in vanilla either, so it starts over.
        const random::Seed128bit seed = stream.xoroshiroState();
        root.putLongArray(name.toString(), std::vector<std::int64_t>{seed.lo, seed.hi});
    }
    static_cast<void>(wire::io::fileIO::writeBytes(
        pathFor(worldRoot), wire::nbt::nbtIO::write(wire::nbt::NbtTag::ofCompound(std::move(root)))));
    dirty_ = false;
}

} // namespace basalt::server::runtime::world
