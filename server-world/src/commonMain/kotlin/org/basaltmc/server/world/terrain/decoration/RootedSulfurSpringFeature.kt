package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier

/** The 26.2 `rooted_sulfur_spring` RootSystemFeature and its nested sulfur-spring selector. */
internal object RootedSulfurSpringFeature {
    fun place(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        templates: (Identifier) -> StructureTemplate?,
        blockAt: (DecorationPos) -> Int,
    ): Boolean {
        val region = Region(chunk, blockAt)
        if (!Blocks.isAir(region[origin])) return false

        var candidate = origin
        for (columnOffset in 0 until ROOT_COLUMN_MAX_HEIGHT) {
            candidate = candidate.copy(y = candidate.y + 1)
            if (
                chunk.heightAt(
                    candidate.x and 15,
                    candidate.z and 15,
                    Chunk.HeightmapType.WORLD_SURFACE,
                ) < candidate.y
            ) {
                return true
            }
            if (!Blocks.isAir(region[candidate]) || !spaceForSpring(region, candidate)) continue

            val below = region[candidate.copy(y = candidate.y - 1)]
            if (isLava(below) || !BlockShapes.isSolidState(below)) return true
            if (!placeSulfurSpring(region, random, candidate, templates)) continue

            placeSulfurRoots(region, random, origin, origin.y + columnOffset)
            placeHangingSulfur(region, random, origin)
            return true
        }
        return true
    }

    /** Required air column plus the four ±8 level probes from RootSystemFeature.spaceForTree. */
    private fun spaceForSpring(
        region: Region,
        candidate: DecorationPos,
    ): Boolean {
        for (distance in 1..REQUIRED_VERTICAL_SPACE) {
            val state = region[candidate.copy(y = candidate.y + distance)]
            if (!Blocks.isAir(state) && !(distance + 1 <= ALLOWED_VERTICAL_WATER && Blocks.hasWaterFluid(state))) {
                return false
            }
        }
        for ((dx, dz) in LEVEL_TEST_DIRECTIONS) {
            val probe = candidate.copy(x = candidate.x + dx, z = candidate.z + dz)
            if (Blocks.isAir(region[probe.copy(y = probe.y - MAX_LEVEL_DEVIATION)])) return false
            if (!Blocks.isAir(region[probe.copy(y = probe.y + MAX_LEVEL_DEVIATION)])) return false
        }
        return true
    }

    /** WeightedRandomSelectorFeature followed by the selected SequenceFeature. */
    private fun placeSulfurSpring(
        region: Region,
        random: WorldgenRandom,
        origin: DecorationPos,
        templates: (Identifier) -> StructureTemplate?,
    ): Boolean {
        val variant = weightedVariant(random)
        var placedTuff = false
        repeat(variant.tuffAttempts) {
            val position =
                DecorationPos(
                    origin.x + triangular(random, variant.radius),
                    origin.y + triangular(random, 3),
                    origin.z + triangular(random, variant.radius),
                )
            val ground = scanDownForSolid(region, position) ?: return@repeat
            region.set(ground, TUFF_STATE_ID)
            placedTuff = true
        }
        if (!placedTuff) return false

        // WeightedList.getRandomOrThrow draws even for its one-entry extra-large list.
        val templateId = variant.templates[random.nextInt(variant.templates.size)]
        val template = templates(templateId) ?: return false
        val rotation = StructureRotation.entries[random.nextInt(StructureRotation.entries.size)]
        val halfX = template.size.x / 2
        val halfZ = template.size.z / 2
        val offset =
            when (rotation) {
                StructureRotation.NONE -> StructurePos(-halfX, 0, -halfZ)
                StructureRotation.CLOCKWISE_90 -> StructurePos(halfZ, 0, -halfX)
                StructureRotation.CLOCKWISE_180 -> StructurePos(halfX, 0, halfZ)
                StructureRotation.COUNTERCLOCKWISE_90 -> StructurePos(-halfZ, 0, halfX)
            }
        return template.placeDecorationInto(
            region.chunk,
            StructurePos(origin.x + offset.x, origin.y - 7, origin.z + offset.z),
            rotation,
            random,
        )
    }

    /** TrapezoidInt(-radius, radius, 0)'s optimized symmetric branch. */
    private fun triangular(
        random: WorldgenRandom,
        radius: Int,
    ): Int = random.nextInt(radius + 1) - random.nextInt(radius + 1)

    /** EnvironmentScanPlacement down with max_steps=4, including the starting and fourth positions. */
    private fun scanDownForSolid(
        region: Region,
        origin: DecorationPos,
    ): DecorationPos? {
        var position = origin
        for (step in 0..ENVIRONMENT_SCAN_STEPS) {
            if (BlockShapes.isSolidState(region[position])) return position
            if (step != ENVIRONMENT_SCAN_STEPS) position = position.copy(y = position.y - 1)
        }
        return null
    }

    private fun weightedVariant(random: WorldgenRandom): Variant {
        var roll = random.nextInt(TOTAL_VARIANT_WEIGHT)
        for (variant in VARIANTS) {
            if (roll < variant.weight) return variant
            roll -= variant.weight
        }
        return VARIANTS.last()
    }

    private fun placeSulfurRoots(
        region: Region,
        random: WorldgenRandom,
        origin: DecorationPos,
        targetHeight: Int,
    ) {
        for (y in origin.y until targetHeight) {
            repeat(ROOT_PLACEMENT_ATTEMPTS) {
                val position =
                    DecorationPos(
                        origin.x + random.nextInt(ROOT_RADIUS) - random.nextInt(ROOT_RADIUS),
                        y,
                        origin.z + random.nextInt(ROOT_RADIUS) - random.nextInt(ROOT_RADIUS),
                    )
                if (Blocks.byStateId(region[position])?.block in ROOT_REPLACEABLE) {
                    region.set(position, SULFUR_STATE_ID)
                }
            }
        }
    }

    private fun placeHangingSulfur(
        region: Region,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        repeat(HANGING_ROOT_PLACEMENT_ATTEMPTS) {
            // Preserve all six nextInt(1) calls made by the real providers even though every offset is zero.
            val position =
                DecorationPos(
                    origin.x + random.nextInt(HANGING_ROOT_RADIUS) - random.nextInt(HANGING_ROOT_RADIUS),
                    origin.y +
                        random.nextInt(HANGING_ROOT_VERTICAL_SPAN) - random.nextInt(HANGING_ROOT_VERTICAL_SPAN),
                    origin.z + random.nextInt(HANGING_ROOT_RADIUS) - random.nextInt(HANGING_ROOT_RADIUS),
                )
            if (!Blocks.isAir(region[position])) return@repeat
            val above = region[position.copy(y = position.y + 1)]
            if (BlockShapes.isFaceSturdy(above, BlockFace.DOWN)) region.set(position, SULFUR_STATE_ID)
        }
    }

    private class Region(
        val chunk: Chunk,
        private val blockAt: (DecorationPos) -> Int,
    ) {
        operator fun get(position: DecorationPos): Int = blockAt(position)

        fun set(
            position: DecorationPos,
            stateId: Int,
        ) {
            val expected = blockAt(position)
            chunk.setDecorationBlockStateId(
                position.x,
                position.y,
                position.z,
                stateId,
                expectedExternalStateId = expected,
            )
        }
    }

    private fun isLava(stateId: Int): Boolean = Blocks.byStateId(stateId)?.block == LAVA_BLOCK

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private data class Variant(
        val weight: Int,
        val tuffAttempts: Int,
        val radius: Int,
        val templates: List<Identifier>,
    )

    private fun templates(
        size: String,
        count: Int,
    ): List<Identifier> = (1..count).map { Identifier.vanilla("spring/sulfur_spring_${size}_$it") }

    private val VARIANTS =
        listOf(
            Variant(200, 64, 7, templates("small", 4)),
            Variant(90, 80, 8, templates("medium", 3)),
            Variant(20, 96, 9, templates("large", 2)),
            Variant(5, 128, 10, templates("extra_large", 1)),
        )
    private val TOTAL_VARIANT_WEIGHT = VARIANTS.sumOf(Variant::weight)
    private val LEVEL_TEST_DIRECTIONS =
        listOf(
            0 to -LEVEL_TEST_DISTANCE,
            LEVEL_TEST_DISTANCE to 0,
            0 to LEVEL_TEST_DISTANCE,
            -LEVEL_TEST_DISTANCE to 0,
        )
    private val ROOT_REPLACEABLE =
        setOf(
            "stone",
            "granite",
            "diorite",
            "andesite",
            "tuff",
            "deepslate",
            "dirt",
            "coarse_dirt",
            "rooted_dirt",
            "mud",
            "muddy_mangrove_roots",
            "moss_block",
            "pale_moss_block",
            "grass_block",
            "podzol",
            "mycelium",
            "terracotta",
            "white_terracotta",
            "orange_terracotta",
            "magenta_terracotta",
            "light_blue_terracotta",
            "yellow_terracotta",
            "lime_terracotta",
            "pink_terracotta",
            "gray_terracotta",
            "light_gray_terracotta",
            "cyan_terracotta",
            "purple_terracotta",
            "blue_terracotta",
            "brown_terracotta",
            "green_terracotta",
            "red_terracotta",
            "black_terracotta",
            "red_sand",
            "clay",
            "gravel",
            "sand",
            "snow_block",
            "powder_snow",
        ).map(Identifier::vanilla).toSet()
    private val TUFF_STATE_ID = state("tuff")
    private val SULFUR_STATE_ID = state("sulfur")
    private val LAVA_BLOCK = Identifier.vanilla("lava")

    private const val ROOT_COLUMN_MAX_HEIGHT = 184
    private const val REQUIRED_VERTICAL_SPACE = 5
    private const val ALLOWED_VERTICAL_WATER = 1
    private const val LEVEL_TEST_DISTANCE = 8
    private const val MAX_LEVEL_DEVIATION = 2
    private const val ROOT_PLACEMENT_ATTEMPTS = 20
    private const val ROOT_RADIUS = 3
    private const val HANGING_ROOT_PLACEMENT_ATTEMPTS = 1
    private const val HANGING_ROOT_RADIUS = 1
    private const val HANGING_ROOT_VERTICAL_SPAN = 1
    private const val ENVIRONMENT_SCAN_STEPS = 4
}
