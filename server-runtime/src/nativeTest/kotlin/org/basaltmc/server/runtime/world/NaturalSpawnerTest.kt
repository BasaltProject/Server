package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Biomes
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

    @Test
    fun randomStartRangeUsesWorldSurfacePlusOneAndClusterLimitsComeFromTheMob() {
        val chunk = assertNotNull(world.state.chunkIfLoaded(0, 0))
        chunk.setBlockStateId(2, 70, 2, state("glass"))

        assertEquals(72, NaturalSpawner.spawnTopY(chunk, 2, 2))
        assertEquals(1, NaturalSpawner.maxClusterSize(Identifier.vanilla("ghast")))
        assertEquals(5, NaturalSpawner.maxClusterSize(Identifier.vanilla("salmon")))
        assertEquals(6, NaturalSpawner.maxClusterSize(Identifier.vanilla("horse")))
        assertEquals(8, NaturalSpawner.maxClusterSize(Identifier.vanilla("cod")))
        assertEquals(4, NaturalSpawner.maxClusterSize(Identifier.vanilla("cow")))
    }

    @Test
    fun everyNonMiscCategoryParticipatesAndCreaturesKeepTheirFourHundredTickInterval() {
        val ordinaryTick = NaturalSpawner.categoriesFor("easy", gameTime = 1)
        val persistentTick = NaturalSpawner.categoriesFor("easy", gameTime = 400)

        assertEquals(MobCategory.entries.filter { it != MobCategory.MISC && !it.persistent }, ordinaryTick)
        assertEquals(MobCategory.entries.filter { it != MobCategory.MISC }, persistentTick)
        assertTrue(MobCategory.WATER_CREATURE in ordinaryTick)
        assertTrue(MobCategory.WATER_AMBIENT in ordinaryTick)
        assertTrue(MobCategory.UNDERGROUND_WATER_CREATURE in ordinaryTick)
        assertTrue(MobCategory.AXOLOTLS in ordinaryTick)
        assertFalse(MobCategory.MONSTER in NaturalSpawner.categoriesFor("peaceful", 1))
    }

    @Test
    fun biomeLookupUsesTheLoadedChunkSoNetherAndFlatWorldsCanSpawn() {
        val chunk = assertNotNull(world.state.chunkIfLoaded(0, 0))
        val netherWastesId = assertNotNull(Biomes.REGISTRY.idOf(Biomes.NETHER_WASTES.id))
        chunk.setBiomeId(
            quartX = 1,
            quartY = (64 - chunk.verticalBounds.minY) shr 2,
            quartZ = 1,
            biomeId = netherWastesId,
        )

        assertEquals(Biomes.NETHER_WASTES.id, NaturalSpawner.biomeAt(world, 4, 64, 4))
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

    @Test
    fun specializedAnimalsUseTheirOwnGroundTags() {
        world.state.setBlockState(2, 70, 2, state("mycelium"))
        world.state.setBlockState(3, 70, 2, state("red_sand"))
        world.state.setBlockState(4, 70, 2, Blocks.STONE.stateId)

        assertTrue(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("mooshroom"),
                MobCategory.CREATURE,
                2,
                71,
                2,
                LegacyRandom(1),
            ),
        )
        assertTrue(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("camel"),
                MobCategory.CREATURE,
                3,
                71,
                2,
                LegacyRandom(1),
            ),
        )
        assertFalse(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("camel"),
                MobCategory.CREATURE,
                4,
                71,
                2,
                LegacyRandom(1),
            ),
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

    @Test
    fun waterMobsUseWaterPlacementRatherThanTheGroundRule() {
        for (y in 58..61) world.state.setBlockState(12, y, 12, state("water"))

        assertTrue(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("cod"),
                MobCategory.WATER_AMBIENT,
                12,
                60,
                12,
                LegacyRandom(1),
            ),
        )
        assertFalse(
            NaturalSpawner.canSpawnAt(world, MobCategory.WATER_AMBIENT, 12, 60, 12, LegacyRandom(1)),
            "the legacy ground placement cannot accidentally accept a water cell",
        )
    }

    @Test
    fun stridersUseTheLavaColumnAndItsAirSurface() {
        for (y in 30..32) world.state.setBlockState(13, y, 13, state("lava"))

        assertTrue(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("strider"),
                MobCategory.CREATURE,
                13,
                30,
                13,
                LegacyRandom(1),
            ),
        )
        world.state.setBlockState(13, 33, 13, Blocks.STONE.stateId)
        assertFalse(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("strider"),
                MobCategory.CREATURE,
                13,
                30,
                13,
                LegacyRandom(1),
            ),
        )
    }

    @Test
    fun slimeChunksAndTheMoonTrackMatchTheOfficialVectors() {
        assertFalse(NaturalSpawner.isSlimeChunk(worldSeed = 0L, chunkX = 0, chunkZ = 0))
        assertTrue(NaturalSpawner.isSlimeChunk(worldSeed = 0L, chunkX = -8, chunkZ = 0))
        assertTrue(NaturalSpawner.isSlimeChunk(worldSeed = 1234L, chunkX = -20, chunkZ = -14))
        assertFalse(NaturalSpawner.isSlimeChunk(worldSeed = 1234L, chunkX = 12, chunkZ = -34))

        assertEquals(0.5f, NaturalSpawner.surfaceSlimeChance(0L))
        assertEquals(0.375f, NaturalSpawner.surfaceSlimeChance(24_000L))
        assertEquals(0.0f, NaturalSpawner.surfaceSlimeChance(96_000L))
        assertEquals(0.375f, NaturalSpawner.surfaceSlimeChance(168_000L))
        assertEquals(0.5f, NaturalSpawner.surfaceSlimeChance(192_000L))
    }

    @Test
    fun slimesUseSwampMoonlightOrTheSeededUndergroundChunkInsteadOfMonsterDarkness() {
        val chunk = assertNotNull(world.state.chunkIfLoaded(0, 0))
        val swampId = assertNotNull(Biomes.REGISTRY.idOf(Biomes.SWAMP.id))
        chunk.setBiomeId(2, (60 - chunk.verticalBounds.minY) shr 2, 2, swampId)
        world.state.setBlockState(8, 59, 8, Blocks.STONE.stateId)
        for (x in 0..15) {
            for (z in 0..15) world.state.setBlockState(x, 63, z, Blocks.STONE.stateId)
        }

        assertTrue(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("slime"),
                MobCategory.MONSTER,
                8,
                60,
                8,
                LegacyRandom(4096L),
            ),
            "full-moon swamp roll 4096 is below 0.5 and its brightness roll is seven",
        )

        val slimeChunkX = -8
        val undergroundX = (slimeChunkX shl 4) + 8
        world.state.getOrGenerateChunk(slimeChunkX, 0)
        world.state.setBlockState(undergroundX, 19, 8, Blocks.STONE.stateId)
        assertTrue(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("slime"),
                MobCategory.MONSTER,
                undergroundX,
                20,
                8,
                LegacyRandom(0L),
            ),
            "seed zero chunk -8,0 is a slime chunk and random seed zero rolls nextInt(10)=0",
        )
        world.state.setBlockState(2, 19, 2, Blocks.STONE.stateId)
        assertFalse(
            NaturalSpawner.canSpawnAt(
                world,
                Identifier.vanilla("slime"),
                MobCategory.MONSTER,
                2,
                20,
                2,
                LegacyRandom(0L),
            ),
            "the same underground roll outside a slime chunk must fail",
        )
    }
}
