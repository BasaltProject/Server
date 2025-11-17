#pragma once

#include <basalt/server/random/PositionalRandomFactory.hpp>
#include <basalt/server/world/noise/NormalNoise.hpp>

#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

/// Resolves a named noise into a correctly-seeded `NormalNoise` the way `Noises.instantiate` does:
/// read `data/minecraft/worldgen/noise/<id>.json` from the operator's own jar, then seed off
/// `random.fromHashOf("minecraft:<id>")`.
///
/// Cached, because every density function referencing the same named noise must get the same
/// instance, matching vanilla's registry-level sharing.
class RealNoiseParameters {
public:
    /// One jar entry as text, or nothing when the jar has no such entry.
    ///
    /// The jar arrives as a callback rather than as `ServerJarAssets`: this needs the operator's own
    /// copy of the game, and a test has no jar to hand it.
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// The octaves one `worldgen/noise/<id>.json` declares. A callback because the reader for that
    /// schema is not ported yet.
    using ParseParameters =
        std::function<std::optional<server::world::noise::NormalNoise::Parameters>(std::string_view json)>;

    RealNoiseParameters(ReadText readText, ParseParameters parseParameters,
                        server::random::PositionalRandomFactory random)
        : readText_(std::move(readText)), parseParameters_(std::move(parseParameters)), random_(random) {}

    RealNoiseParameters(const RealNoiseParameters&) = delete;
    RealNoiseParameters& operator=(const RealNoiseParameters&) = delete;

    /// The named noise, or nothing when the jar ships no such file or it does not parse.
    [[nodiscard]] const server::world::noise::NormalNoise* get(std::string_view id);

private:
    ReadText readText_;
    ParseParameters parseParameters_;
    server::random::PositionalRandomFactory random_;
    // One instance is shared by every generation worker, so the cache has one owner and it is this
    // lock. Map nodes never move, so a noise handed out stays valid while another thread inserts.
    std::mutex mutex_;
    std::map<std::string, server::world::noise::NormalNoise, std::less<>> cache_;
};

} // namespace basalt::server::runtime::world
