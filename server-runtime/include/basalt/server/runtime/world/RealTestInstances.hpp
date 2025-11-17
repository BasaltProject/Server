#pragma once

#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// One gametest instance, as `data/<ns>/test_instance/<path>.json` declares it.
///
/// Only the shape this server can run is modelled: a `minecraft:function` test, which is a function
/// that fails by failing. That is the one kind the vanilla jar itself ships, and the one a datapack
/// can write without the block-assertion API.
struct TestInstance {
    wire::common::Identifier id;
    wire::common::Identifier type;
    std::optional<wire::common::Identifier> function;
    std::int32_t maxTicks = 0;
    bool required = true;

    [[nodiscard]] bool isFunctionTest() const;
};

/// Test instances from the operator's own `server.jar`, plus whatever datapacks add later.
class RealTestInstances {
public:
    /// One jar entry as text, or nothing when the jar has no such entry.
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    /// Every entry name under a prefix, which is how a whole registry directory is listed.
    using EntriesUnder = std::function<std::vector<std::string>(std::string_view prefix)>;

    /// The jar arrives as callbacks rather than as `ServerJarAssets`: this needs the operator's own
    /// copy of the game, and a test has no jar to hand it.
    struct Jar {
        ReadText readText;
        EntriesUnder entriesUnder;
    };

    explicit RealTestInstances(const Jar& jar, wire::common::Logger logger = wire::common::Logger{"Tests"});

    [[nodiscard]] const std::vector<TestInstance>& all() const noexcept { return all_; }

    /// The instance with this id, or nothing when nothing declares it.
    [[nodiscard]] const TestInstance* find(const wire::common::Identifier& id) const;

private:
    wire::common::Logger logger_;
    std::vector<TestInstance> all_;
};

} // namespace basalt::server::runtime::world
