#include "SelectorParser.hpp"

#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/command/EntitySelector.hpp>
#include <basalt/server/runtime/scoreboard/Scoreboard.hpp>
#include <basalt/server/runtime/scoreboard/Teams.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <expected>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace basalt::server::runtime::command {

Parsed<EntitySelector> EntitySelector::parse(StringReader& reader, const CommandSource& source,
                                             const CommandEnvironment& environment) {
    static_cast<void>(source);
    selectorParser::SelectorParser parser{reader, environment};
    return parser.parse();
}

} // namespace basalt::server::runtime::command
