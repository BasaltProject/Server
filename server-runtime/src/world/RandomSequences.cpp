#include <basalt/server/random/RandomSupport.hpp>
#include <basalt/server/runtime/world/RandomSequences.hpp>

namespace basalt::server::runtime::world {

random::RandomSource& RandomSequences::of(const wire::common::Identifier& name) {
    const std::lock_guard<std::mutex> held{mutex_};
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

} // namespace basalt::server::runtime::world
