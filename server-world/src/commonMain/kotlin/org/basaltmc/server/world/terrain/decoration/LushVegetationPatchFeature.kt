package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasWaterFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.wire.common.Identifier

/** Configured moss/clay vegetation-patch bodies used by the lush-cave step-9 shells. */
internal object LushVegetationPatchFeature {
    fun placeCeilingMoss(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean = placePatch(chunk, random, origin, CEILING_MOSS)

    fun placeFloorMoss(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean = placePatch(chunk, random, origin, FLOOR_MOSS)

    fun placeClay(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean = placePatch(chunk, random, origin, if (random.nextBoolean()) CLAY else WATERLOGGED_CLAY)

    private fun placePatch(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        config: PatchConfig,
    ): Boolean {
        // VegetationPatchFeature adds one to each independently sampled UniformInt(4..7).
        val xRadius = 5 + random.nextInt(4)
        val zRadius = 5 + random.nextInt(4)
        val surface = ArrayList<DecorationPos>((xRadius * 2 + 1) * (zRadius * 2 + 1))
        for (dx in -xRadius..xRadius) {
            val xEdge = dx == -xRadius || dx == xRadius
            for (dz in -zRadius..zRadius) {
                val zEdge = dz == -zRadius || dz == zRadius
                if (xEdge && zEdge) continue
                if ((xEdge || zEdge) && random.nextFloat() > config.extraEdgeColumnChance) continue

                val candidate = origin.copy(x = origin.x + dx, z = origin.z + dz)
                val ground = findGround(chunk, candidate, config) ?: continue
                val extraDepth =
                    if (config.maxDepth == config.minDepth) {
                        0
                    } else {
                        random.nextInt(config.maxDepth - config.minDepth + 1)
                    }
                var depth = config.minDepth + extraDepth
                if (config.extraBottomBlockChance > 0.0f && random.nextFloat() < config.extraBottomBlockChance) {
                    depth++
                }
                if (placeGround(chunk, ground, depth, config)) surface += ground
            }
        }

        val vegetationSurface =
            if (config.waterlogged) waterlogEnclosedSurface(chunk, surface) else surface
        for (ground in javaHashSetOrder(vegetationSurface)) {
            if (random.nextFloat() >= config.vegetationChance) continue
            val vegetationOrigin =
                if (config.waterlogged) ground else ground.copy(y = ground.y - config.inwards)
            when (config.vegetation) {
                Vegetation.CAVE_VINE -> CaveVineColumnFeature.place(chunk, random, vegetationOrigin, inMoss = true)
                Vegetation.MOSS -> placeMossVegetation(chunk, random, vegetationOrigin)
                Vegetation.DRIPLEAF -> {
                    val placed = placeDripleafAt(chunk, random, vegetationOrigin)
                    if (config.waterlogged && placed) waterlogPlacedVegetation(chunk, vegetationOrigin)
                }
            }
        }
        return surface.isNotEmpty()
    }

    private fun findGround(
        chunk: Chunk,
        origin: DecorationPos,
        config: PatchConfig,
    ): DecorationPos? {
        var position = origin
        var offset = 0
        while (Blocks.isAir(blockAt(chunk, position)) && offset < config.verticalRange) {
            position = position.copy(y = position.y + config.inwards)
            offset++
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return null
        }
        offset = 0
        while (!Blocks.isAir(blockAt(chunk, position)) && offset < config.verticalRange) {
            position = position.copy(y = position.y - config.inwards)
            offset++
            if (position.y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) return null
        }
        val ground = position.copy(y = position.y + config.inwards)
        if (!Blocks.isAir(blockAt(chunk, position))) return null
        val supportFace = if (config.inwards < 0) BlockFace.UP else BlockFace.DOWN
        if (!BlockShapes.isFaceSturdy(blockAt(chunk, ground), supportFace)) return null
        return ground
    }

    private fun placeGround(
        chunk: Chunk,
        origin: DecorationPos,
        depth: Int,
        config: PatchConfig,
    ): Boolean {
        var position = origin
        repeat(depth) { index ->
            val current = blockAt(chunk, position)
            if (Blocks.byStateId(current)?.block != Blocks.byStateId(config.groundStateId)?.block) {
                if (Blocks.byStateId(current)?.block !in config.replaceable) return index != 0
                set(chunk, position, config.groundStateId)
                position = position.copy(y = position.y + config.inwards)
            }
        }
        return true
    }

    private fun waterlogEnclosedSurface(
        chunk: Chunk,
        surface: List<DecorationPos>,
    ): List<DecorationPos> {
        val enclosed =
            surface.filter { position ->
                EXPOSURE_DIRECTIONS.none { direction ->
                    val neighbour =
                        position.copy(
                            x = position.x + direction.face.stepX,
                            y = position.y + direction.face.stepY,
                            z = position.z + direction.face.stepZ,
                        )
                    !BlockShapes.isFaceSturdy(blockAt(chunk, neighbour), direction.face.opposite)
                }
            }
        enclosed.forEach { position -> set(chunk, position, Blocks.WATER.stateId) }
        return enclosed
    }

    private fun placeMossVegetation(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ) {
        val roll = random.nextInt(96)
        val stateId =
            when {
                roll < 4 -> FLOWERING_AZALEA_STATE_ID
                roll < 11 -> AZALEA_STATE_ID
                roll < 36 -> MOSS_CARPET_STATE_ID
                roll < 86 -> SHORT_GRASS_STATE_ID
                else -> TALL_GRASS_STATE_ID
            }
        SimpleBlockFeature({ _, _ -> stateId }, isDoublePlant = stateId == TALL_GRASS_STATE_ID)
            .place(chunk, random, origin)
    }

    internal fun placeDripleafAt(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
    ): Boolean {
        val selected = random.nextInt(5)
        return if (selected == 0) {
            val facing = FACING_NAMES[random.nextInt(FACING_NAMES.size)]
            placeSmallDripleaf(chunk, origin, facing)
        } else {
            val facing = BIG_DRIPLEAF_FACING_NAMES[selected - 1]
            placeBigDripleaf(chunk, random, origin, facing)
        }
    }

    private fun placeSmallDripleaf(
        chunk: Chunk,
        origin: DecorationPos,
        facing: String,
    ): Boolean {
        val current = blockAt(chunk, origin)
        val below = blockAt(chunk, origin.copy(y = origin.y - 1))
        if (!smallDripleafSurvives(current, below)) return false
        val upperOrigin = origin.copy(y = origin.y + 1)
        val upperCurrent = blockAt(chunk, upperOrigin)
        if (!Blocks.isAir(upperCurrent)) return false
        val lower = smallDripleafState(facing, "lower", Blocks.hasWaterFluid(current))
        val upper = smallDripleafState(facing, "upper", Blocks.hasWaterFluid(upperCurrent))
        set(chunk, origin, lower)
        set(chunk, upperOrigin, upper)
        return true
    }

    private fun placeBigDripleaf(
        chunk: Chunk,
        random: WorldgenRandom,
        origin: DecorationPos,
        facing: String,
    ): Boolean {
        var stemHeight = if (random.nextInt(3) < 2) random.nextInt(5) else 0
        var tipHeight = 1
        val totalHeight = stemHeight + tipHeight
        var validHeight = totalHeight
        var next = origin.copy(y = origin.y + 1)
        for (distance in 0 until totalHeight) {
            if (!canReplaceWithDripleaf(blockAt(chunk, next))) {
                validHeight = distance
                break
            }
            next = next.copy(y = next.y + 1)
        }
        var remove = totalHeight - validHeight
        val removedStems = minOf(stemHeight, remove)
        stemHeight -= removedStems
        remove -= removedStems
        tipHeight -= minOf(tipHeight, remove)

        var position = origin
        repeat(stemHeight) {
            set(chunk, position, bigDripleafStemState(facing))
            position = position.copy(y = position.y + 1)
        }
        repeat(tipHeight) {
            set(chunk, position, bigDripleafState(facing))
            position = position.copy(y = position.y + 1)
        }
        return true
    }

    private fun waterlogPlacedVegetation(
        chunk: Chunk,
        origin: DecorationPos,
    ) {
        val stateId = blockAt(chunk, origin)
        val state = Blocks.byStateId(stateId) ?: return
        if (state.properties["waterlogged"] != "false") return
        val waterlogged = Blocks.stateIdOf(state.block, state.properties + ("waterlogged" to "true")) ?: return
        set(chunk, origin, waterlogged)
    }

    private fun smallDripleafSurvives(
        currentStateId: Int,
        belowStateId: Int,
    ): Boolean {
        val belowPath = Blocks.byStateId(belowStateId)?.block?.path
        return belowPath in SUPPORTS_SMALL_DRIPLEAF_PATHS ||
            (hasSourceWaterFluid(currentStateId) && belowPath in SUPPORTS_VEGETATION_PATHS)
    }

    private fun smallDripleafState(
        facing: String,
        half: String,
        waterlogged: Boolean,
    ): Int =
        Blocks.stateIdOf(
            Identifier.vanilla("small_dripleaf"),
            mapOf("facing" to facing, "half" to half, "waterlogged" to waterlogged.toString()),
        ) ?: error("unknown small_dripleaf state")

    private fun hasSourceWaterFluid(stateId: Int): Boolean =
        Blocks.hasWaterFluid(stateId) && stateId !in Blocks.WATER.stateId + 1..Blocks.WATER.stateId + 15

    private fun canReplaceWithDripleaf(stateId: Int): Boolean =
        Blocks.isAir(stateId) || Blocks.byStateId(stateId)?.block?.path == "water"

    private fun bigDripleafStemState(facing: String): Int =
        Blocks.stateIdOf(
            Identifier.vanilla("big_dripleaf_stem"),
            mapOf("facing" to facing, "waterlogged" to "false"),
        ) ?: error("unknown big_dripleaf_stem state")

    private fun bigDripleafState(facing: String): Int =
        Blocks.stateIdOf(
            Identifier.vanilla("big_dripleaf"),
            mapOf("facing" to facing, "tilt" to "none", "waterlogged" to "false"),
        ) ?: error("unknown big_dripleaf state")

    private fun blockAt(
        chunk: Chunk,
        position: DecorationPos,
    ): Int = chunk.getDecorationBlockStateId(position.x, position.y, position.z)

    private fun set(
        chunk: Chunk,
        position: DecorationPos,
        stateId: Int,
    ) {
        chunk.setDecorationBlockStateId(position.x, position.y, position.z, stateId)
    }

    /** HashMap bucket iteration used by vanilla's HashSet<BlockPos>. */
    private fun javaHashSetOrder(positions: List<DecorationPos>): Sequence<DecorationPos> {
        if (positions.isEmpty()) return emptySequence()
        var capacity = 16
        while (positions.size > capacity * 3 / 4) capacity *= 2
        return sequence {
            for (bucket in 0 until capacity) {
                for (position in positions) {
                    val hash = (position.y + position.z * 31) * 31 + position.x
                    val spread = hash xor (hash ushr 16)
                    if (spread and (capacity - 1) == bucket) yield(position)
                }
            }
        }
    }

    private data class PatchConfig(
        val groundStateId: Int,
        val replaceable: Set<Identifier>,
        val minDepth: Int,
        val maxDepth: Int,
        val extraBottomBlockChance: Float,
        val verticalRange: Int,
        val vegetationChance: Float,
        val extraEdgeColumnChance: Float,
        val inwards: Int,
        val vegetation: Vegetation,
        val waterlogged: Boolean = false,
    )

    private enum class Vegetation {
        CAVE_VINE,
        MOSS,
        DRIPLEAF,
    }

    private data class ExposureDirection(
        val face: BlockFace,
    )

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val MOSS_REPLACEABLE =
        setOf(
            "stone",
            "granite",
            "diorite",
            "andesite",
            "tuff",
            "deepslate",
            "cave_vines_plant",
            "cave_vines",
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
        ).map(Identifier::vanilla).toSet()
    private val LUSH_GROUND_REPLACEABLE =
        MOSS_REPLACEABLE + setOf(Identifier.vanilla("clay"), Identifier.vanilla("gravel"), Identifier.vanilla("sand"))
    private val CEILING_MOSS =
        PatchConfig(state("moss_block"), MOSS_REPLACEABLE, 1, 2, 0.0f, 5, 0.08f, 0.3f, 1, Vegetation.CAVE_VINE)
    private val FLOOR_MOSS =
        PatchConfig(state("moss_block"), MOSS_REPLACEABLE, 1, 1, 0.0f, 5, 0.8f, 0.3f, -1, Vegetation.MOSS)
    private val CLAY =
        PatchConfig(state("clay"), LUSH_GROUND_REPLACEABLE, 3, 3, 0.8f, 2, 0.05f, 0.7f, -1, Vegetation.DRIPLEAF)
    private val WATERLOGGED_CLAY =
        PatchConfig(
            state("clay"),
            LUSH_GROUND_REPLACEABLE,
            3,
            3,
            0.8f,
            5,
            0.1f,
            0.7f,
            -1,
            Vegetation.DRIPLEAF,
            waterlogged = true,
        )
    private val EXPOSURE_DIRECTIONS =
        listOf(BlockFace.NORTH, BlockFace.EAST, BlockFace.SOUTH, BlockFace.WEST, BlockFace.DOWN)
            .map(::ExposureDirection)
    private val FACING_NAMES = listOf("east", "west", "north", "south")
    private val BIG_DRIPLEAF_FACING_NAMES = listOf("east", "west", "south", "north")
    private val SUPPORTS_SMALL_DRIPLEAF_PATHS = setOf("clay", "moss_block")
    private val SUPPORTS_VEGETATION_PATHS =
        setOf(
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
            "farmland",
        )
    private val FLOWERING_AZALEA_STATE_ID = state("flowering_azalea")
    private val AZALEA_STATE_ID = state("azalea")
    private val MOSS_CARPET_STATE_ID = state("moss_carpet")
    private val SHORT_GRASS_STATE_ID = state("short_grass")
    private val TALL_GRASS_STATE_ID = state("tall_grass")
}
