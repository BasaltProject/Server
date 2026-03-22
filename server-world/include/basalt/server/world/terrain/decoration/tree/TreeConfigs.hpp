#pragma once

#include <basalt/server/world/terrain/decoration/tree/TreeFeature.hpp>

namespace basalt::server::world::terrain::decoration::tree {

/// The real per-tree configurations, from the jar's own `TreeFeatures` bootstrap rather than from a
/// datapack dump: the serialized form loses values the runtime object keeps (see the cherry's corner
/// hole chance).
namespace treeConfigs {

[[nodiscard]] const TreeConfiguration& oak();
[[nodiscard]] const TreeConfiguration& azalea();
[[nodiscard]] const TreeConfiguration& swampOak();
[[nodiscard]] const TreeConfiguration& birch();
[[nodiscard]] const TreeConfiguration& superBirch();
[[nodiscard]] const TreeConfiguration& fancyOak();
[[nodiscard]] const TreeConfiguration& jungleTree();
[[nodiscard]] const TreeConfiguration& jungleBush();
[[nodiscard]] const TreeConfiguration& megaJungle();
[[nodiscard]] const TreeConfiguration& spruce();
[[nodiscard]] const TreeConfiguration& pine();
[[nodiscard]] const TreeConfiguration& acacia();
[[nodiscard]] const TreeConfiguration& darkOak();
[[nodiscard]] const TreeConfiguration& paleOak();
[[nodiscard]] const TreeConfiguration& paleOakCreaking();
[[nodiscard]] const TreeConfiguration& megaSpruce();
[[nodiscard]] const TreeConfiguration& megaPine();
[[nodiscard]] const TreeConfiguration& mangrove();
[[nodiscard]] const TreeConfiguration& tallMangrove();
[[nodiscard]] const TreeConfiguration& cherry();

[[nodiscard]] const TreeConfiguration& redPoplar();
[[nodiscard]] const TreeConfiguration& orangePoplar();
[[nodiscard]] const TreeConfiguration& yellowPoplar();
[[nodiscard]] const TreeConfiguration& redPoplarLeafLitter();
[[nodiscard]] const TreeConfiguration& orangePoplarLeafLitter();
[[nodiscard]] const TreeConfiguration& yellowPoplarLeafLitter();
[[nodiscard]] const TreeConfiguration& oakLeafLitter();
[[nodiscard]] const TreeConfiguration& birchLeafLitter();
[[nodiscard]] const TreeConfiguration& fancyOakLeafLitter();
[[nodiscard]] const TreeConfiguration& darkOakLeafLitter();
[[nodiscard]] const TreeConfiguration& oakBees0002LeafLitter();
[[nodiscard]] const TreeConfiguration& birchBees0002LeafLitter();
[[nodiscard]] const TreeConfiguration& fancyOakBees0002LeafLitter();
[[nodiscard]] const TreeConfiguration& oakBees002();
[[nodiscard]] const TreeConfiguration& birchBees002();
[[nodiscard]] const TreeConfiguration& fancyOakBees002();
[[nodiscard]] const TreeConfiguration& oakBees005();
[[nodiscard]] const TreeConfiguration& fancyOakBees005();
[[nodiscard]] const TreeConfiguration& birchBees0002();
[[nodiscard]] const TreeConfiguration& superBirchBees0002();
[[nodiscard]] const TreeConfiguration& fancyOakBees();
[[nodiscard]] const TreeConfiguration& superBirchBees();
[[nodiscard]] const TreeConfiguration& cherryBees005();

} // namespace treeConfigs

} // namespace basalt::server::world::terrain::decoration::tree
