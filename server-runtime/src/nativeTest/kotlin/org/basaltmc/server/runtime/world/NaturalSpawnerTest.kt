package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/** parity: `NaturalSpawner`: the caps, the weights, and what a spot has to be for a mob to stand on it. */
class NaturalSpawnerTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("spawner", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/spawner",
            Logger("NaturalSpawnerTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(name: String): Int =
        checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), emptyMap()))

    /** parity: `max * chunks / 289`, which is what keeps a busy server from filling up. */
    @Test
    fun theCapsAreVanillasOwn() {
        assertEquals(70, NaturalSpawner.capFor(MobCategory.MONSTER, 289))
        assertEquals(10, NaturalSpawner.capFor(MobCategory.CREATURE, 289))
        assertEquals(0, NaturalSpawner.capFor(MobCategory.MONSTER, 0), "nothing simulated, nothing spawned")
        assertEquals(140, NaturalSpawner.capFor(MobCategory.MONSTER, 578), "twice the chunks, twice the mobs")
    }

    /** parity: `WeightedRandomList.getRandom`: the heavier entry comes up more often. */
    @Test
    fun theWeightsDecideWhatAppears() {
        val entries =
            listOf(
                SpawnerEntry(Identifier.vanilla("sheep"), weight = 90, minCount = 4, maxCount = 4),
                SpawnerEntry(Identifier.vanilla("horse"), weight = 10, minCount = 2, maxCount = 6),
            )

        var sheep = 0
        repeat(200) { seed ->
            val picked = NaturalSpawner.pick(entries, LegacyRandom(seed.toLong()))
            if (picked?.type == Identifier.vanilla("sheep")) sheep++
        }

        assertTrue(sheep > 150, "nine in ten should be sheep, got $sheep in 200")
        assertEquals(null, NaturalSpawner.pick(emptyList(), LegacyRandom(1)), "an empty biome offers nothing")
    }

    /** parity: `SpawnPlacementTypes.ON_GROUND`: solid under it, room to stand in. */
    @Test
    fun itNeedsGroundAndRoom() {
        world.state.setBlockState(4, 70, 4, Blocks.STONE.stateId)

        assertTrue(NaturalSpawner.canSpawnAt(world, MobCategory.MONSTER, 4, 71, 4, LegacyRandom(1)))
        assertFalse(
            NaturalSpawner.canSpawnAt(world, MobCategory.MONSTER, 4, 72, 4, LegacyRandom(1)),
            "nothing under it",
        )

        world.state.setBlockState(4, 72, 4, Blocks.STONE.stateId)
        assertFalse(
            NaturalSpawner.canSpawnAt(world, MobCategory.MONSTER, 4, 71, 4, LegacyRandom(1)),
            "no room for its head",
        )
    }

    /** parity: `Animal.checkAnimalSpawnRules`: grass under it, and light above eight. */
    @Test
    fun anAnimalNeedsGrassAndLight() {
        world.state.setBlockState(6, 70, 6, state("grass_block"))
        world.state.setBlockState(7, 70, 6, Blocks.STONE.stateId)

        // A void world has full sky light everywhere above ground, which is what an animal needs.
        assertTrue(NaturalSpawner.canSpawnAt(world, MobCategory.CREATURE, 6, 71, 6, LegacyRandom(1)))
        assertFalse(
            NaturalSpawner.canSpawnAt(world, MobCategory.CREATURE, 7, 71, 6, LegacyRandom(1)),
            "stone is not something an animal spawns on",
        )
    }

    /** parity: `Monster.isDarkEnoughToSpawn`, which is why a lit room stays empty. */
    @Test
    fun aMonsterNeedsTheDark() {
        world.state.setBlockState(9, 70, 9, Blocks.STONE.stateId)
        // A ceiling, so the sky light below it is nothing at all.
        for (x in 8..10) {
            for (z in 8..10) world.state.setBlockState(x, 74, z, Blocks.STONE.stateId)
        }

        assertNotNull(world.state.getBlockState(9, 71, 9))
        assertTrue(
            NaturalSpawner.canSpawnAt(world, MobCategory.MONSTER, 9, 71, 9, LegacyRandom(1)),
            "under a ceiling with no light, a monster fits",
        )
    }
}
