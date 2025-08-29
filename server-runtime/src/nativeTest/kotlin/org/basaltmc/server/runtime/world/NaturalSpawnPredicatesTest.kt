package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.EntityTypeShapes
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** Durable vectors for the data-driven animal placement rules shared by both natural spawners. */
class NaturalSpawnPredicatesTest {
    @Test
    fun armadilloUsesOnlyTheBadlandsTerracottaTag() {
        assertTrue(allows("armadillo", "terracotta"))
        assertTrue(allows("armadillo", "light_gray_terracotta"))
        assertFalse(allows("armadillo", "blue_terracotta"))
    }

    @Test
    fun polarBearAlternateGroundDependsOnBiome() {
        assertTrue(allows("polar_bear", "ice", biome = "frozen_ocean"))
        assertFalse(allows("polar_bear", "grass_block", biome = "frozen_ocean"))
        assertTrue(allows("polar_bear", "grass_block", biome = "snowy_plains"))
        assertFalse(allows("polar_bear", "ice", biome = "snowy_plains"))
    }

    @Test
    fun parrotRecognizesNetherStemLogTagsWithoutAcceptingMushroomStem() {
        assertTrue(allows("parrot", "crimson_hyphae"))
        assertTrue(allows("parrot", "stripped_warped_stem"))
        assertFalse(allows("parrot", "mushroom_stem"))
    }

    @Test
    fun placementAndSurfaceMonsterOverridesMatchRegistrations() {
        assertFalse(NaturalSpawnPredicates.usesGroundPlacement("fox"))
        assertFalse(NaturalSpawnPredicates.usesGroundPlacement("panda"))
        assertTrue(NaturalSpawnPredicates.usesGroundPlacement("chicken"))
        assertTrue(NaturalSpawnPredicates.isSurfaceMonster("camel_husk"))
    }

    @Test
    fun entityTypeDangerousBlockImmunitiesMatchTheirTags() {
        assertTrue(EntityTypeShapes.isImmuneTo(Identifier.vanilla("fox"), "sweet_berry_bush"))
        assertTrue(EntityTypeShapes.isImmuneTo(Identifier.vanilla("stray"), "powder_snow"))
        assertTrue(EntityTypeShapes.isImmuneTo(Identifier.vanilla("wither_skeleton"), "wither_rose"))
        assertFalse(EntityTypeShapes.isImmuneTo(Identifier.vanilla("zombie"), "powder_snow"))
    }

    @Test
    fun naturalSpawnDistanceUsesTheEntityTypeCategoryAndOverride() {
        val beyond = 128.0 * 128.0 + 1.0
        assertFalse(NaturalSpawner.canSpawnAtDistance(Identifier.vanilla("zombie"), MobCategory.CREATURE, beyond))
        assertTrue(
            NaturalSpawner.canSpawnAtDistance(Identifier.vanilla("zombie"), MobCategory.CREATURE, beyond - 1.0),
        )
        assertTrue(NaturalSpawner.canSpawnAtDistance(Identifier.vanilla("cow"), MobCategory.MONSTER, beyond))
        assertTrue(NaturalSpawner.canSpawnAtDistance(Identifier.vanilla("pillager"), MobCategory.MONSTER, beyond))
    }

    @Test
    fun displacedPackRevalidatesSpawnerDataWithoutComparingItsWeight() {
        val cow = Identifier.vanilla("cow")
        val selected = SpawnerEntry(cow, weight = 8, minCount = 2, maxCount = 4)
        assertTrue(NaturalSpawner.containsSpawnData(listOf(SpawnerEntry(cow, 1, 2, 4)), selected))
        assertFalse(NaturalSpawner.containsSpawnData(listOf(SpawnerEntry(cow, 8, 1, 4)), selected))
        assertFalse(
            NaturalSpawner.containsSpawnData(
                listOf(SpawnerEntry(Identifier.vanilla("pig"), 8, 2, 4)),
                selected,
            ),
        )
    }

    @Test
    fun waterPlacementAndGlowSquidRequireTheirExactBlockStates() {
        assertTrue(NaturalSpawnPredicates.waterPlacementAllows(atHasWater = true, aboveConductsRedstone = false))
        assertFalse(NaturalSpawnPredicates.waterPlacementAllows(atHasWater = true, aboveConductsRedstone = true))
        assertTrue(NaturalSpawnPredicates.glowSquidAllows(30, 63, 0, atIsWaterBlock = true))
        assertFalse(NaturalSpawnPredicates.glowSquidAllows(30, 63, 0, atIsWaterBlock = false))
    }

    @Test
    fun guardianRollShortCircuitsSkyAndStillRequiresTwoWaterBlocks() {
        assertTrue(
            NaturalSpawnPredicates.guardianAllows(0, "normal", true, true) {
                error("the successful one-in-twenty roll must skip the sky scan")
            },
        )
        assertFalse(NaturalSpawnPredicates.guardianAllows(1, "normal", true, true) { true })
        assertTrue(NaturalSpawnPredicates.guardianAllows(1, "normal", true, true) { false })
        assertFalse(NaturalSpawnPredicates.guardianAllows(0, "normal", true, false) { false })
    }

    @Test
    fun spawnAabbUsesCubeSafetyScaleAndFullBlockVolume() {
        val slime = Identifier.vanilla("slime")
        assertEquals(2.08, EntityTypeShapes.spawnWidthOf(slime), absoluteTolerance = 0.000_001)
        assertEquals(2.08, EntityTypeShapes.spawnHeightOf(slime), absoluteTolerance = 0.000_001)

        val zombie = Identifier.vanilla("zombie")
        val air = requireNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("air"), emptyMap()))
        val stone = requireNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("stone"), emptyMap()))
        val water = requireNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla("water"), emptyMap()))
        assertFalse(
            NaturalSpawnObstruction.blocksAllow(zombie, 0.5, 64.0, 0.5) { x, y, z ->
                if (x == 0 && y == 64 && z == 0) stone else air
            },
        )
        assertTrue(NaturalSpawnObstruction.blocksAllow(zombie, 0.5, 64.0, 0.5) { _, _, _ -> air })
        assertTrue(
            NaturalSpawnObstruction.blocksAllow(slime, 0.5, 64.0, 0.5) { x, y, z ->
                if (x == 1 && y == 64 && z == 0) water else air
            },
        )
        assertTrue(NaturalSpawnObstruction.allowsLiquids(Identifier.vanilla("drowned")))
        assertFalse(NaturalSpawnObstruction.allowsLiquids(zombie))
    }

    private fun allows(
        type: String,
        ground: String,
        biome: String = "plains",
    ): Boolean =
        NaturalSpawnPredicates.allows(
            type = type,
            ground = ground,
            groundIsLeaves = false,
            biome = biome,
            y = 64,
            seaLevel = 63,
        )
}
