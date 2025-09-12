package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.BlockRot
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureProcessorList
import org.basaltmc.server.world.structure.StructureProcessorRule
import org.basaltmc.server.world.structure.StructureRuleTest
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier

/**
 * The Overworld UNDERGROUND_STRUCTURES nodes, step 3, in the order the biomes force: `fossil_upper`,
 * `fossil_lower`, `monster_room`, `monster_room_deep`. The fossils need a template loader, and without
 * one their draws do not happen, which is a different world: hence it is threaded all the way here.
 */
public object OverworldUndergroundStructures {
    public fun decorate(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        templates: TemplateLookup? = null,
        externalOceanFloorHeight: ((x: Int, z: Int) -> Int?)? = null,
    ) {
        if (templates != null) {
            for (fossil in FOSSILS) {
                decorateFossil(chunk, seed, environment, fossil, templates, externalOceanFloorHeight)
            }
        }
        for (spec in SPECS) decorateFeature(chunk, seed, environment, spec)
    }

    /**
     * // parity: `fossil_upper` and `fossil_lower`, whose chain is a rarity filter and not a count:
     * one attempt in 64 chunks, and the draws below only happen in that chunk.
     */
    private fun decorateFossil(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        fossil: FossilSpec,
        templates: TemplateLookup,
        externalOceanFloorHeight: ((x: Int, z: Int) -> Int?)?,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, fossil.index, UNDERGROUND_STRUCTURES_STEP)
        if (random.nextFloat() >= 1.0f / RARITY) return
        val x = chunk.chunkX * 16 + random.nextInt(16)
        val z = chunk.chunkZ * 16 + random.nextInt(16)
        val y = fossil.minY + random.nextInt(fossil.maxY - fossil.minY + 1)
        if (environment.biomeAt(x, y, z).substringAfter(':') !in FOSSIL_BIOMES) return
        FossilFeature.place(
            chunk,
            random,
            StructurePos(x, y, z),
            fossil.spec,
            templates,
        ) { columnX, columnZ ->
            if (columnX shr 4 == chunk.chunkX && columnZ shr 4 == chunk.chunkZ) {
                // parity: FossilFeature reads `OCEAN_FLOOR_WG`, the frozen map, not the live one:
                // an ore blob or another fossil placed first must not raise the floor it buries to.
                chunk.worldgenHeightAt(columnX and 15, columnZ and 15, Chunk.HeightmapType.OCEAN_FLOOR)
            } else {
                externalOceanFloorHeight?.invoke(columnX, columnZ)
            }
        }
    }

    private fun decorateFeature(
        chunk: Chunk,
        seed: Long,
        environment: SurfaceRuleEnvironment,
        spec: RoomSpec,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, spec.index, UNDERGROUND_STRUCTURES_STEP)
        repeat(spec.count) {
            val x = chunk.chunkX * 16 + random.nextInt(16)
            val z = chunk.chunkZ * 16 + random.nextInt(16)
            val y = spec.minY + random.nextInt(spec.maxY - spec.minY + 1)
            if (environment.biomeAt(x, y, z).substringAfter(':') !in ALL_BIOMES) return@repeat
            StandaloneFeatures.placeAt(chunk, random, DecorationPos(x, y, z), "monster_room")
        }
    }

    private class RoomSpec(
        val index: Int,
        val count: Int,
        val minY: Int,
        val maxY: Int,
    )

    private val SPECS =
        listOf(
            // `below_top 0`, the highest block the world can generate.
            RoomSpec(index = 2, count = 10, minY = 0, maxY = BELOW_TOP),
            // `above_bottom 6` up to y -1, the deep pass.
            RoomSpec(index = 3, count = 4, minY = WorldHeight.MIN_Y + 6, maxY = -1),
        )

    private class FossilSpec(
        val index: Int,
        val minY: Int,
        val maxY: Int,
        val spec: FossilFeature.Spec,
    )

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: Blocks.AIR.stateId

    /** // parity: `#minecraft:features_cannot_replace`, the tag all three fossil lists protect. */
    private val PROTECTED =
        setOf(
            "bedrock",
            "spawner",
            "chest",
            "end_portal_frame",
            "reinforced_deepslate",
            "trial_spawner",
            "vault",
        ).mapNotNull { Blocks.REGISTRY.byIdentifier(Identifier.vanilla(it))?.stateId }.toSet()

    /** // parity: `processor_list/fossil_rot`, the bones, which keep nine blocks in ten. */
    private val FOSSIL_ROT = StructureProcessorList(emptyList(), BlockRot(0.9f, null), PROTECTED)

    /** // parity: `processor_list/fossil_coal`, one block in ten of the ore overlay. */
    private val FOSSIL_COAL = StructureProcessorList(emptyList(), BlockRot(0.1f, null), PROTECTED)

    /** // parity: `processor_list/fossil_diamonds`, the same tenth with its coal swapped out. */
    private val FOSSIL_DIAMONDS =
        StructureProcessorList(
            listOf(
                StructureProcessorRule(
                    StructureRuleTest.BlockMatch(state("coal_ore")),
                    null,
                    state("deepslate_diamond_ore"),
                ),
            ),
            BlockRot(0.1f, null),
            PROTECTED,
        )

    private val FOSSIL_BASE =
        listOf(
            "fossil/spine_1",
            "fossil/spine_2",
            "fossil/spine_3",
            "fossil/spine_4",
            "fossil/skull_1",
            "fossil/skull_2",
            "fossil/skull_3",
            "fossil/skull_4",
        )

    private val FOSSIL_OVERLAY = FOSSIL_BASE.map { it + "_coal" }

    private val FOSSILS =
        listOf(
            // `fossil_upper`: absolute 0 up to `below_top 0`, with the coal overlay.
            FossilSpec(
                index = 0,
                minY = 0,
                maxY = BELOW_TOP,
                spec = FossilFeature.Spec(FOSSIL_BASE, FOSSIL_OVERLAY, FOSSIL_ROT, FOSSIL_COAL, MAX_EMPTY_CORNERS),
            ),
            // `fossil_lower`: `above_bottom 0` up to y -8, where the coal becomes diamond.
            FossilSpec(
                index = 1,
                minY = WorldHeight.MIN_Y,
                maxY = -8,
                spec =
                    FossilFeature.Spec(
                        FOSSIL_BASE,
                        FOSSIL_OVERLAY,
                        FOSSIL_ROT,
                        FOSSIL_DIAMONDS,
                        MAX_EMPTY_CORNERS,
                    ),
            ),
        )

    /**
     * // parity: the only three biomes whose generation settings list `fossil_upper` and
     * `fossil_lower`. The monster rooms share the step but not the biomes, which is why this is
     * not the rooms' own set.
     */
    private val FOSSIL_BIOMES = setOf("desert", "swamp", "mangrove_swamp")

    private const val RARITY = 64
    private const val MAX_EMPTY_CORNERS = 4
    private const val UNDERGROUND_STRUCTURES_STEP = 3
    private const val BELOW_TOP = 319
}
