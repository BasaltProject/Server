package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.server.world.structure.TemplateBlockProcessorChain
import org.basaltmc.server.world.structure.TemplateBlockProcessors
import kotlin.concurrent.AtomicReference

/** Where a ruined portal sits relative to the ground, which decides almost everything else. */
public enum class RuinedPortalPlacement {
    ON_LAND_SURFACE,
    PARTLY_BURIED,
    ON_OCEAN_FLOOR,
    IN_MOUNTAIN,
    UNDERGROUND,
    IN_NETHER,
}

/** // parity: RuinedPortalStructure.Setup, one weighted way of placing a portal. */
public class RuinedPortalSetup(
    public val placement: RuinedPortalPlacement,
    public val airPocketProbability: Float,
    public val mossiness: Float,
    public val overgrown: Boolean,
    public val vines: Boolean,
    public val canBeCold: Boolean,
    public val replaceWithBlackstone: Boolean,
    public val weight: Float,
)

/** One of the seven ruined portal structures, with the biomes it may start in. */
public class RuinedPortalVariant(
    public val name: String,
    public val biomes: Set<String>,
    public val setups: List<RuinedPortalSetup>,
)

/**
 * Overworld ruined portals. // parity: the `ruined_portals` set is a plain `random_spread`, spacing
 * 40, separation 15, salt 34222645. The seven variants share the cell by weight, first accepted wins.
 */
public class RuinedPortalStructureGenerator(
    private val variants: List<RuinedPortalVariant>,
    private val templateOf: (String) -> StructureTemplate?,
    private val biomeAt: (x: Int, y: Int, z: Int) -> String,
    private val surfaceHeightAt: (x: Int, z: Int) -> Int,
    private val oceanFloorAt: (x: Int, z: Int) -> Int,
    private val isSolidAt: (x: Int, y: Int, z: Int) -> Boolean,
) {
    private val grid = StructurePlacementGrid(spacing = 40, separation = 15, salt = SALT)
    private val placed = AtomicReference<Map<Long, Portal?>>(emptyMap())

    /** Writes into [chunk] whatever of a nearby ruined portal lands there. */
    public fun generateInto(
        chunk: Chunk,
        seed: Long,
    ) {
        val clip =
            StructureBoundingBox(
                chunk.chunkX * 16,
                WorldHeight.MIN_Y,
                chunk.chunkZ * 16,
                chunk.chunkX * 16 + 15,
                WorldHeight.MAX_Y,
                chunk.chunkZ * 16 + 15,
            )
        for (startX in chunk.chunkX - REACH_CHUNKS..chunk.chunkX + REACH_CHUNKS) {
            for (startZ in chunk.chunkZ - REACH_CHUNKS..chunk.chunkZ + REACH_CHUNKS) {
                val portal = portalAt(seed, startX, startZ) ?: continue
                if (!portal.box.intersects(clip)) continue
                place(portal, chunk, clip, seed)
            }
        }
    }

    private fun place(
        portal: Portal,
        chunk: Chunk,
        clip: StructureBoundingBox,
        seed: Long,
    ) {
        val setup = portal.setup
        val processors =
            buildList {
                // parity: the BlockIgnoreProcessor comes first, and which one depends on the air
                // pocket. Without it the template's structure void is written as air, which buries
                // the water a cave portal is meant to sit in.
                add(
                    if (portal.airPocket) {
                        TemplateBlockProcessors.IGNORE_STRUCTURE_BLOCK
                    } else {
                        TemplateBlockProcessors.IGNORE_STRUCTURE_AND_AIR
                    },
                )
                add(TemplateBlockProcessors.age(setup.mossiness))
                add(TemplateBlockProcessors.PROTECT_FEATURES)
                add(TemplateBlockProcessors.LAVA_SUBMERGED)
                if (setup.replaceWithBlackstone) add(TemplateBlockProcessors.BLACKSTONE_REPLACE)
            }
        // parity: `RuinedPortalPiece.postProcess` takes the decoration random it is handed, not one of its
        // own, and passes it to `placeInWorld`, the netherrack, the drip columns and the vines. Each of the
        // seven variants owns its index within `surface_structures`, so the stream depends on which was drawn.
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val decorationSeed = random.setDecorationSeed(seed, chunk.chunkX * 16, chunk.chunkZ * 16)
        random.setFeatureSeed(decorationSeed, structureIndexOf(portal.variantName), SURFACE_STRUCTURES_STEP)
        portal.template.placeInto(
            chunk = chunk,
            position = portal.origin,
            rotation = portal.rotation,
            mirror = portal.mirror,
            pivot = portal.pivot,
            clipTo = clip,
            blockProcessor = TemplateBlockProcessorChain(processors),
            random = random,
        )
        spreadNetherrack(portal, chunk, clip, random)
        dripColumnsBelowPortal(portal, chunk, clip, random)
        if (setup.vines || setup.overgrown) {
            // parity: BlockPos.betweenClosedStream walks x fastest, then y, then z, and every
            // non-air block draws a direction whether or not a vine ends up on it.
            for (z in portal.box.minZ..portal.box.maxZ) {
                for (y in portal.box.minY..portal.box.maxY) {
                    for (x in portal.box.minX..portal.box.maxX) {
                        if (setup.vines) maybeAddVines(chunk, random, clip, x, y, z)
                        if (setup.overgrown) maybeAddLeavesAbove(chunk, random, clip, x, y, z)
                    }
                }
            }
        }
    }

    /** // parity: RuinedPortalPiece.maybeAddVines. */
    private fun maybeAddVines(
        chunk: Chunk,
        random: WorldgenRandom,
        clip: StructureBoundingBox,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val state = chunk.getDecorationBlockStateId(x, y, z)
        if (Blocks.isAir(state) || Blocks.byStateId(state)?.block?.path == "vine") return
        val face = HORIZONTAL[random.nextInt(HORIZONTAL.size)]
        val neighbourX = x + face.stepX
        val neighbourZ = z + face.stepZ
        val neighbour = chunk.getDecorationBlockStateId(neighbourX, y, neighbourZ)
        if (!Blocks.isAir(neighbour)) return
        if (!BlockShapes.isCollisionFaceFull(state, face)) return
        if (!clip.isInside(StructurePos(neighbourX, y, neighbourZ))) return
        val vine = vineTowards(face.opposite) ?: return
        chunk.setDecorationBlockStateId(neighbourX, y, neighbourZ, vine, expectedExternalStateId = neighbour)
    }

    /** // parity: RuinedPortalPiece.maybeAddLeavesAbove. The draw happens either way. */
    private fun maybeAddLeavesAbove(
        chunk: Chunk,
        random: WorldgenRandom,
        clip: StructureBoundingBox,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (random.nextFloat() >= LEAVES_CHANCE) return
        if (chunk.getDecorationBlockStateId(x, y, z) != NETHERRACK) return
        val above = chunk.getDecorationBlockStateId(x, y + 1, z)
        if (!Blocks.isAir(above)) return
        if (!clip.isInside(StructurePos(x, y + 1, z))) return
        chunk.setDecorationBlockStateId(x, y + 1, z, JUNGLE_LEAVES, expectedExternalStateId = above)
    }

    /** // parity: RuinedPortalPiece.addNetherrackDripColumnsBelowPortal. */
    private fun dripColumnsBelowPortal(
        portal: Portal,
        chunk: Chunk,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        for (x in portal.box.minX + 1 until portal.box.maxX) {
            for (z in portal.box.minZ + 1 until portal.box.maxZ) {
                if (chunk.getDecorationBlockStateId(x, portal.box.minY, z) != NETHERRACK) continue
                addDripColumn(chunk, random, portal, clip, x, portal.box.minY - 1, z)
            }
        }
    }

    /**
     * // parity: RuinedPortalPiece.getHeightMapType. Only the ocean floor variant reads
     * `OCEAN_FLOOR_WG`; every other placement, the swamp one included, reads `WORLD_SURFACE_WG`,
     * which over water is the surface of the water.
     */
    private fun columnHeight(
        placement: RuinedPortalPlacement,
        x: Int,
        z: Int,
    ): Int = if (placement == RuinedPortalPlacement.ON_OCEAN_FLOOR) oceanFloorAt(x, z) else surfaceHeightAt(x, z)

    /**
     * The `_WG` height as the level would answer it. Inside the chunk being written that is the
     * frozen snapshot; outside it the apron never places anything, so the plain column stands in.
     */
    private fun frozenHeight(
        chunk: Chunk,
        placement: RuinedPortalPlacement,
        x: Int,
        z: Int,
    ): Int {
        if (x shr 4 != chunk.chunkX || z shr 4 != chunk.chunkZ) return columnHeight(placement, x, z)
        val type =
            if (placement == RuinedPortalPlacement.ON_OCEAN_FLOOR) {
                Chunk.HeightmapType.OCEAN_FLOOR
            } else {
                Chunk.HeightmapType.WORLD_SURFACE
            }
        return chunk.worldgenHeightAt(x and 15, z and 15, type)
    }

    /**
     * // parity: RuinedPortalPiece.spreadNetherrack, the apron of netherrack around the portal that
     * fades out with taxicab distance from its centre.
     */
    private fun spreadNetherrack(
        portal: Portal,
        chunk: Chunk,
        clip: StructureBoundingBox,
        random: WorldgenRandom,
    ) {
        val followsSurface =
            portal.setup.placement == RuinedPortalPlacement.ON_LAND_SURFACE ||
                portal.setup.placement == RuinedPortalPlacement.ON_OCEAN_FLOOR
        val centreX = (portal.box.minX + portal.box.maxX) / 2
        val centreZ = (portal.box.minZ + portal.box.maxZ) / 2
        val averageWidth =
            ((portal.box.maxX - portal.box.minX + 1) + (portal.box.maxZ - portal.box.minZ + 1)) / 2
        val adjustment = random.nextInt(maxOf(1, 8 - averageWidth / 2))
        val reach = NETHERRACK_BY_DISTANCE.size
        for (x in centreX - reach..centreX + reach) {
            for (z in centreZ - reach..centreZ + reach) {
                val distance = maxOf(0, kotlin.math.abs(x - centreX) + kotlin.math.abs(z - centreZ) + adjustment)
                if (distance >= reach) continue
                if (random.nextDouble() >= NETHERRACK_BY_DISTANCE[distance]) continue
                val surfaceY = frozenHeight(chunk, portal.setup.placement, x, z) - 1
                val y = if (followsSurface) surfaceY else minOf(portal.box.minY, surfaceY)
                if (kotlin.math.abs(y - portal.box.minY) > MAX_Y_DIFFERENCE) continue
                if (!clip.isInside(StructurePos(x, y, z))) continue
                val existing = chunk.getDecorationBlockStateId(x, y, z)
                if (!canBeReplacedByNetherrack(existing, portal)) continue
                placeNetherrackOrMagma(chunk, random, portal, x, y, z)
                if (portal.setup.overgrown) maybeAddLeavesAbove(chunk, random, clip, x, y, z)
                addDripColumn(chunk, random, portal, clip, x, y - 1, z)
            }
        }
    }

    private fun addDripColumn(
        chunk: Chunk,
        random: WorldgenRandom,
        portal: Portal,
        clip: StructureBoundingBox,
        x: Int,
        startY: Int,
        z: Int,
    ) {
        var y = startY
        if (clip.isInside(StructurePos(x, y, z))) placeNetherrackOrMagma(chunk, random, portal, x, y, z)
        var remaining = MAX_DRIP_LENGTH
        while (remaining > 0 && random.nextFloat() < DRIP_CONTINUES) {
            y--
            remaining--
            if (clip.isInside(StructurePos(x, y, z))) placeNetherrackOrMagma(chunk, random, portal, x, y, z)
        }
    }

    private fun placeNetherrackOrMagma(
        chunk: Chunk,
        random: WorldgenRandom,
        portal: Portal,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val state =
            if (!portal.cold && random.nextFloat() < MAGMA_CHANCE) MAGMA else NETHERRACK
        chunk.setDecorationBlockStateId(
            x,
            y,
            z,
            state,
            expectedExternalStateId = chunk.getDecorationBlockStateId(x, y, z),
        )
    }

    private fun canBeReplacedByNetherrack(
        stateId: Int,
        portal: Portal,
    ): Boolean {
        if (Blocks.isAir(stateId)) return false
        val path = Blocks.byStateId(stateId)?.block?.path ?: return false
        if (path == "obsidian" || path in FEATURES_CANNOT_REPLACE) return false
        return portal.setup.placement == RuinedPortalPlacement.IN_NETHER || path != "lava"
    }

    private fun portalAt(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Portal? {
        val key = (chunkX.toLong() shl 32) or (chunkZ.toLong() and 0xFFFFFFFFL)
        placed.value[key]?.let { return it }
        if (placed.value.containsKey(key)) return null
        val portal = if (grid.isStructureChunk(seed, chunkX, chunkZ)) assemble(seed, chunkX, chunkZ) else null
        while (true) {
            val current = placed.value
            if (current.containsKey(key)) return current[key]
            val next = if (current.size >= CACHE_CAPACITY) mapOf(key to portal) else current + (key to portal)
            if (placed.compareAndSet(current, next)) return portal
        }
    }

    /**
     * // parity: ChunkGenerator's retry loop. A variant is drawn by weight, its whole generation point
     * computed, and only then is the biome checked, at that point rather than at the chunk. A refused
     * variant is dropped and the next drawn, each attempt starting from the same random state.
     */
    private fun assemble(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Portal? {
        val pickRandom = WorldgenRandom(LegacyRandom(0L))
        pickRandom.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val remaining = variants.toMutableList()
        while (remaining.isNotEmpty()) {
            val chosen = remaining[pickRandom.nextInt(remaining.size)]
            val portal = generationPoint(seed, chunkX, chunkZ, chosen)
            // The tag keeps its namespace, and so does the biome name, so neither side is stripped.
            if (portal != null && biomeAt(portal.origin.x, portal.origin.y, portal.origin.z) in chosen.biomes) {
                return portal
            }
            remaining.remove(chosen)
        }
        return null
    }

    /** // parity: RuinedPortalStructure.findGenerationPoint, whose every draw is in this order. */
    private fun generationPoint(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
        variant: RuinedPortalVariant,
    ): Portal? {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(seed, chunkX, chunkZ)
        val setup = chooseSetup(random, variant) ?: return null
        val airPocket = sample(random, setup.airPocketProbability)
        val giant = random.nextFloat() < GIANT_CHANCE
        val names = if (giant) GIANT_PORTALS else PORTALS
        val templateName = names[random.nextInt(names.size)]
        val template = templateOf(templateName) ?: return null
        val rotation = StructureRotation.entries[random.nextInt(StructureRotation.entries.size)]
        val mirror = if (random.nextFloat() < 0.5f) StructureMirror.NONE else StructureMirror.FRONT_BACK
        val pivot = StructurePos(template.size.x / 2, 0, template.size.z / 2)
        val base = StructurePos(chunkX * 16, 0, chunkZ * 16)
        val box = template.getBoundingBox(base, rotation, pivot, mirror)
        val centreX = (box.minX + box.maxX) / 2
        val centreZ = (box.minZ + box.maxZ) / 2
        val surfaceY = columnHeight(setup.placement, centreX, centreZ) - 1
        val ySpan = box.maxY - box.minY + 1
        val y = suitableY(random, setup.placement, airPocket, surfaceY, ySpan, box)
        val origin = StructurePos(base.x, y, base.z)
        val cold = setup.canBeCold && isCold(biomeAt(origin.x, origin.y, origin.z))
        return Portal(
            chunkX,
            chunkZ,
            variant.name,
            setup,
            template,
            origin,
            rotation,
            mirror,
            pivot,
            template.getBoundingBox(origin, rotation, pivot, mirror),
            airPocket,
            cold,
        )
    }

    private fun chooseSetup(
        random: WorldgenRandom,
        variant: RuinedPortalVariant,
    ): RuinedPortalSetup? {
        if (variant.setups.size == 1) return variant.setups.first()
        val total = variant.setups.fold(0.0f) { sum, s -> sum + s.weight }
        var pick = random.nextFloat()
        for (setup in variant.setups) {
            pick -= setup.weight / total
            if (pick < 0.0f) return setup
        }
        return null
    }

    private fun suitableY(
        random: WorldgenRandom,
        placement: RuinedPortalPlacement,
        airPocket: Boolean,
        surfaceY: Int,
        ySpan: Int,
        box: StructureBoundingBox,
    ): Int {
        val minY = WorldHeight.MIN_Y + MIN_Y_INDEX
        val start =
            when (placement) {
                RuinedPortalPlacement.IN_NETHER ->
                    when {
                        airPocket -> between(random, 32, 100)
                        random.nextFloat() < 0.5f -> between(random, 27, 29)
                        else -> between(random, 29, 100)
                    }
                RuinedPortalPlacement.IN_MOUNTAIN -> withinInterval(random, 70, surfaceY - ySpan)
                RuinedPortalPlacement.UNDERGROUND -> withinInterval(random, minY, surfaceY - ySpan)
                RuinedPortalPlacement.PARTLY_BURIED -> surfaceY - ySpan + between(random, 2, 8)
                else -> surfaceY
            }
        val corners =
            listOf(
                box.minX to box.minZ,
                box.maxX to box.minZ,
                box.minX to box.maxZ,
                box.maxX to box.maxZ,
            )
        var y = start
        while (y > minY) {
            var solidCorners = 0
            for ((x, z) in corners) {
                if (isSolidAt(x, y, z)) {
                    solidCorners++
                    if (solidCorners == 3) return y
                }
            }
            y--
        }
        return y
    }

    private fun isCold(biome: String): Boolean = biome.substringAfter(':') in COLD_BIOMES

    private fun sample(
        random: WorldgenRandom,
        limit: Float,
    ): Boolean =
        when (limit) {
            0.0f -> false
            1.0f -> true
            else -> random.nextFloat() < limit
        }

    private fun between(
        random: WorldgenRandom,
        min: Int,
        max: Int,
    ): Int = min + random.nextInt(max - min + 1)

    private fun withinInterval(
        random: WorldgenRandom,
        preferredMin: Int,
        max: Int,
    ): Int = if (preferredMin < max) between(random, preferredMin, max) else max

    private class Portal(
        val chunkX: Int,
        val chunkZ: Int,
        /** Which of the seven variants this is, which decides its index within the step. */
        val variantName: String,
        val setup: RuinedPortalSetup,
        val template: StructureTemplate,
        val origin: StructurePos,
        val rotation: StructureRotation,
        val mirror: StructureMirror,
        val pivot: StructurePos,
        val box: StructureBoundingBox,
        val airPocket: Boolean,
        val cold: Boolean,
    )

    /**
     * // parity: the position of each variant among the 26 structures of `surface_structures`, in
     * registry order. They share a structure set but not an index, so they do not share a stream.
     */
    private fun structureIndexOf(name: String): Int = STRUCTURE_INDICES[name] ?: DEFAULT_STRUCTURE_INDEX

    private companion object {
        const val SALT = 34222645
        const val SURFACE_STRUCTURES_STEP = 4
        const val DEFAULT_STRUCTURE_INDEX = 10

        val STRUCTURE_INDICES =
            mapOf(
                "ruined_portal" to 10,
                "ruined_portal_desert" to 11,
                "ruined_portal_jungle" to 12,
                "ruined_portal_mountain" to 13,
                "ruined_portal_nether" to 14,
                "ruined_portal_ocean" to 15,
                "ruined_portal_swamp" to 16,
            )
        const val GIANT_CHANCE = 0.05f
        const val MIN_Y_INDEX = 15
        const val MAX_Y_DIFFERENCE = 3
        const val MAX_DRIP_LENGTH = 8
        const val DRIP_CONTINUES = 0.5f
        const val MAGMA_CHANCE = 0.07f

        /** A giant portal is 20 wide at most, so two chunks out covers every block it can reach. */
        const val REACH_CHUNKS = 3
        const val CACHE_CAPACITY = 64

        const val LEAVES_CHANCE = 0.5f

        val MAGMA = blockState("magma_block")
        val NETHERRACK = blockState("netherrack")
        val JUNGLE_LEAVES =
            Blocks.stateIdOf(
                org.basaltmc.wire.common.Identifier
                    .vanilla("jungle_leaves"),
                mapOf("persistent" to "true"),
            ) ?: blockState("jungle_leaves")

        /** // parity: Direction.Plane.HORIZONTAL, whose order the direction draw depends on. */
        val HORIZONTAL = arrayOf(BlockFace.NORTH, BlockFace.EAST, BlockFace.SOUTH, BlockFace.WEST)

        fun vineTowards(face: BlockFace): Int? =
            Blocks.stateIdOf(
                org.basaltmc.wire.common.Identifier
                    .vanilla("vine"),
                mapOf(face.name.lowercase() to "true"),
            )

        fun blockState(path: String): Int =
            Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla(path),
                )?.stateId
                ?: Blocks.AIR.stateId

        val NETHERRACK_BY_DISTANCE =
            floatArrayOf(1f, 1f, 1f, 1f, 1f, 1f, 1f, 0.9f, 0.9f, 0.8f, 0.7f, 0.6f, 0.4f, 0.2f)

        val PORTALS = (1..10).map { "ruined_portal/portal_$it" }
        val GIANT_PORTALS = (1..3).map { "ruined_portal/giant_portal_$it" }

        val FEATURES_CANNOT_REPLACE =
            setOf("bedrock", "spawner", "chest", "end_portal_frame", "reinforced_deepslate", "trial_spawner", "vault")

        /** // parity: `Biome.coldEnoughToSnow`, narrowed to the Overworld biomes that never thaw. */
        val COLD_BIOMES =
            setOf(
                "snowy_plains",
                "ice_spikes",
                "snowy_taiga",
                "snowy_beach",
                "snowy_slopes",
                "frozen_peaks",
                "jagged_peaks",
                "grove",
                "frozen_river",
                "frozen_ocean",
                "deep_frozen_ocean",
            )
    }
}
