#include <basalt/server/random/RandomSupport.hpp>
#include <basalt/server/runtime/world/RandomSequences.hpp>
#include <basalt/server/runtime/world/SavedDataFiles.hpp>
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
constexpr std::string_view kName = "random_sequences";

} // namespace

random::RandomSource& RandomSequences::of(const wire::common::Identifier& name) {
    const std::lock_guard<std::mutex> held{mutex_};
    dirty_ = true;
    const auto found = streams_.find(name);
    if (found != streams_.end()) {
        return found->second;
    }
    return streams_.emplace(name, create(name, salt_, includeWorldSeed_, includeSequenceId_)).first->second;
}

random::RandomSource RandomSequences::create(const wire::common::Identifier& name, const std::int32_t salt,
                                             const bool includeWorldSeed, const bool includeSequenceId) const {
    // parity: `RandomSequences.createSequence` and `RandomSequence.createSequence`.
    random::Seed128bit seed = random::upgradeSeedTo128bitUnmixed((includeWorldSeed ? worldSeed_ : 0) ^ salt);
    if (includeSequenceId) {
        seed = seed.xorWith(random::seedFromHashOf(name.toString()));
    }
    return random::RandomSource::xoroshiro(seed.mixed());
}

void RandomSequences::reset(const wire::common::Identifier& name) {
    reset(name, salt_, includeWorldSeed_, includeSequenceId_);
}

void RandomSequences::reset(const wire::common::Identifier& name, const std::int32_t salt, const bool includeWorldSeed,
                            const bool includeSequenceId) {
    const std::lock_guard<std::mutex> held{mutex_};
    dirty_ = true;
    streams_.insert_or_assign(name, create(name, salt, includeWorldSeed, includeSequenceId));
}

std::int32_t RandomSequences::clear() {
    const std::lock_guard<std::mutex> held{mutex_};
    dirty_ = true;
    const auto count = static_cast<std::int32_t>(streams_.size());
    streams_.clear();
    return count;
}

void RandomSequences::setSeedDefaults(const std::int32_t salt, const bool includeWorldSeed,
                                      const bool includeSequenceId) {
    const std::lock_guard<std::mutex> held{mutex_};
    dirty_ = true;
    salt_ = salt;
    includeWorldSeed_ = includeWorldSeed;
    includeSequenceId_ = includeSequenceId;
}

void RandomSequences::load(const std::string_view worldRoot) {
    const std::lock_guard<std::mutex> held{mutex_};
    const std::optional<wire::nbt::NbtCompound> data = savedData::read(worldRoot, kName);
    if (!data.has_value()) {
        return;
    }
    salt_ = data->getInt("salt", 0);
    includeWorldSeed_ = !data->contains("include_world_seed") || data->getBoolean("include_world_seed");
    includeSequenceId_ = !data->contains("include_sequence_id") || data->getBoolean("include_sequence_id");
    const wire::nbt::NbtCompound* sequences = data->getCompound("sequences");
    if (sequences == nullptr) {
        return;
    }
    for (const std::string& name : sequences->names()) {
        const wire::nbt::NbtCompound* one = sequences->getCompound(name);
        const std::span<const std::int64_t> seed =
            one == nullptr ? std::span<const std::int64_t>{} : one->getLongArray("source");
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
    wire::nbt::NbtCompound sequences;
    for (const auto& [name, stream] : streams_) {
        // parity: `Xoroshiro128PlusPlus.CODEC`.
        const random::Seed128bit seed = stream.xoroshiroState();
        wire::nbt::NbtCompound one;
        one.putLongArray("source", std::vector<std::int64_t>{seed.lo, seed.hi});
        sequences.set(name.toString(), wire::nbt::NbtTag::ofCompound(std::move(one)));
    }
    wire::nbt::NbtCompound data;
    data.putInt("salt", salt_);
    data.putBoolean("include_world_seed", includeWorldSeed_);
    data.putBoolean("include_sequence_id", includeSequenceId_);
    data.set("sequences", wire::nbt::NbtTag::ofCompound(std::move(sequences)));
    static_cast<void>(savedData::write(worldRoot, kName, std::move(data)));
    dirty_ = false;
}

} // namespace basalt::server::runtime::world
