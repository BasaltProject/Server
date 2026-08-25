package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.isVineState
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.decoration.DecorationPos
import org.basaltmc.server.world.terrain.decoration.PaleMossDecoration
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature.BlockPos
import org.basaltmc.server.world.terrain.decoration.tree.TreeFeature.TreeDecoratorContext
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.NbtType
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

/** // parity: AlterGroundDecorator with the beneath_tree_podzol_replaceable rule provider. */
internal fun TreeFeature.alterGroundToPodzol(
    chunk: Chunk,
    random: WorldgenRandom,
    logs: List<BlockPos>,
    roots: List<BlockPos>,
) {
    val blockPositions =
        when {
            roots.isEmpty() -> logs
            logs.isNotEmpty() && roots.first().y == logs.first().y -> logs + roots
            else -> roots
        }
    if (blockPositions.isEmpty()) return
    val minY = blockPositions.first().y
    for (log in blockPositions.filter { it.y == minY }) {
        placePodzolCircle(chunk, log.x - 1, log.y, log.z - 1)
        placePodzolCircle(chunk, log.x + 2, log.y, log.z - 1)
        placePodzolCircle(chunk, log.x - 1, log.y, log.z + 2)
        placePodzolCircle(chunk, log.x + 2, log.y, log.z + 2)
        repeat(5) {
            val placement = random.nextInt(64)
            val x = placement % 8
            val z = placement / 8
            if (x == 0 || x == 7 || z == 0 || z == 7) {
                placePodzolCircle(chunk, log.x - 3 + x, log.y, log.z - 3 + z)
            }
        }
    }
}

internal fun TreeFeature.decorateTrunkVines(
    chunk: Chunk,
    random: WorldgenRandom,
    logs: List<BlockPos>,
) {
    for (log in logs) {
        for (vine in TRUNK_VINES) {
            if (random.nextInt(3) > 0 && isAir(chunk, log.x + vine.dx, log.y, log.z + vine.dz)) {
                setBlock(chunk, log.x + vine.dx, log.y, log.z + vine.dz, defaultVineStateId - vine.faceBit)
            }
        }
    }
}

internal fun TreeFeature.decorateLeafVines(
    chunk: Chunk,
    random: WorldgenRandom,
    leaves: List<BlockPos>,
    probability: Float,
) {
    for (leaf in leaves) {
        for (vine in TRUNK_VINES) {
            if (random.nextFloat() < probability && isAir(chunk, leaf.x + vine.dx, leaf.y, leaf.z + vine.dz)) {
                addHangingVine(chunk, leaf.x + vine.dx, leaf.y, leaf.z + vine.dz, vine.faceBit)
            }
        }
    }
}

internal fun TreeFeature.decorateCocoa(
    chunk: Chunk,
    random: WorldgenRandom,
    logs: List<BlockPos>,
    probability: Float,
) {
    if (random.nextFloat() >= probability || logs.isEmpty()) return
    val treeY = logs.first().y
    for (log in logs) {
        if (log.y - treeY > 2) continue
        for (cocoa in COCOA_DIRECTIONS) {
            if (random.nextFloat() <= 0.25f) {
                val x = log.x + cocoa.dx
                val z = log.z + cocoa.dz
                if (isAir(chunk, x, log.y, z)) {
                    val age = random.nextInt(3)
                    setBlock(chunk, x, log.y, z, defaultCocoaStateId + age * 4 + cocoa.facingOffset)
                }
            }
        }
    }
}

internal fun TreeFeature.decoratePaleMoss(
    chunk: Chunk,
    random: WorldgenRandom,
    mossyCarpetRandom: BasaltRandom,
    context: TreeDecoratorContext,
    decorator: TreeDecoratorConfiguration.PaleMoss,
) {
    val shuffledLogs = context.logs.toMutableList()
    shuffle(shuffledLogs, random)
    if (shuffledLogs.isEmpty()) return
    val origin = shuffledLogs.minBy(BlockPos::y)
    if (random.nextFloat() < decorator.groundProbability) {
        PaleMossDecoration.placePatchAt(
            chunk,
            random,
            DecorationPos(origin.x, origin.y + 1, origin.z),
            mossyCarpetRandom,
        )
    }
    for (log in context.logs) {
        if (random.nextFloat() < decorator.trunkProbability && isAir(chunk, log.x, log.y - 1, log.z)) {
            addPaleMossHanger(chunk, random, log.x, log.y - 1, log.z)
        }
    }
    for (leaf in context.leaves) {
        if (random.nextFloat() < decorator.leavesProbability && isAir(chunk, leaf.x, leaf.y - 1, leaf.z)) {
            addPaleMossHanger(chunk, random, leaf.x, leaf.y - 1, leaf.z)
        }
    }
}

internal fun TreeFeature.addPaleMossHanger(
    chunk: Chunk,
    random: WorldgenRandom,
    x: Int,
    startY: Int,
    z: Int,
) {
    var y = startY
    while (isAir(chunk, x, y - 1, z) && random.nextFloat() >= 0.5f) {
        setBlock(chunk, x, y, z, paleHangingMossTipFalseStateId)
        y--
    }
    setBlock(chunk, x, y, z, paleHangingMossTipTrueStateId)
}

internal fun TreeFeature.decorateCreakingHeart(
    chunk: Chunk,
    random: WorldgenRandom,
    logs: List<BlockPos>,
    probability: Float,
) {
    if (logs.isEmpty() || random.nextFloat() >= probability) return
    val candidates = logs.toMutableList()
    shuffle(candidates, random)
    val target =
        candidates.firstOrNull { position ->
            BLOCK_NEIGHBOURS.all { (dx, dy, dz) ->
                val y = position.y + dy
                y in WorldHeight.MIN_Y..WorldHeight.MAX_Y &&
                    Blocks
                        .byStateId(chunk.getDecorationBlockStateId(position.x + dx, y, position.z + dz))
                        ?.block
                        ?.path
                        ?.let(LOG_BLOCK_PATHS::contains) == true
            }
        } ?: return
    setBlock(chunk, target.x, target.y, target.z, naturalDormantCreakingHeartStateId)
    chunk.setDecorationBlockEntity(
        target.x,
        target.y,
        target.z,
        naturalDormantCreakingHeartStateId,
        Identifier.vanilla("creaking_heart"),
        NbtCompound(),
    )
}

/** // parity: AttachedToLeavesDecorator used by both mangrove tree configurations. */
internal fun TreeFeature.decorateMangrovePropagules(
    chunk: Chunk,
    random: WorldgenRandom,
    leaves: List<BlockPos>,
    decorator: TreeDecoratorConfiguration.MangrovePropagules,
) {
    val blacklist = mutableSetOf<BlockPos>()
    val candidates = leaves.toMutableList()
    shuffle(candidates, random)
    for (leaf in candidates) {
        random.nextInt(1) // Util.getRandom over the single DOWN direction.
        val placement = BlockPos(leaf.x, leaf.y - 1, leaf.z)
        if (placement in blacklist || random.nextFloat() >= decorator.probability) continue
        if ((1..decorator.requiredEmptyBlocks).any { !isAir(chunk, leaf.x, leaf.y - it, leaf.z) }) continue
        for (x in placement.x - decorator.exclusionRadiusXZ..placement.x + decorator.exclusionRadiusXZ) {
            for (y in placement.y - decorator.exclusionRadiusY..placement.y + decorator.exclusionRadiusY) {
                for (z in placement.z - decorator.exclusionRadiusXZ..placement.z + decorator.exclusionRadiusXZ) {
                    blacklist += BlockPos(x, y, z)
                }
            }
        }
        val age = random.nextInt(5)
        setBlock(chunk, placement.x, placement.y, placement.z, hangingMangrovePropaguleAge0StateId + age * 8)
    }
}

/** // parity: PlaceOnGroundDecorator with VegetationFeatures.leafLitterPatchBuilder. */
internal fun TreeFeature.decorateGroundLeafLitter(
    chunk: Chunk,
    random: WorldgenRandom,
    context: TreeDecoratorContext,
    decorator: TreeDecoratorConfiguration.GroundLeafLitter,
) {
    val supportPositions = lowestTrunkOrRootPositions(context)
    if (supportPositions.isEmpty()) return
    val minY = supportPositions.first().y
    var minX = supportPositions.first().x
    var maxX = minX
    var minZ = supportPositions.first().z
    var maxZ = minZ
    for (position in supportPositions) {
        if (position.y != minY) continue
        minX = min(minX, position.x)
        maxX = max(maxX, position.x)
        minZ = min(minZ, position.z)
        maxZ = max(maxZ, position.z)
    }
    minX -= decorator.radius
    maxX += decorator.radius
    minZ -= decorator.radius
    maxZ += decorator.radius
    val minSampleY = minY - decorator.height
    val maxSampleY = minY + decorator.height

    repeat(decorator.tries) {
        val x = minX + random.nextInt(maxX - minX + 1)
        val y = minSampleY + random.nextInt(maxSampleY - minSampleY + 1)
        val z = minZ + random.nextInt(maxZ - minZ + 1)
        val aboveY = y + 1
        if (y !in WorldHeight.MIN_Y..WorldHeight.MAX_Y ||
            aboveY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y
        ) {
            return@repeat
        }
        val aboveStateId = chunk.getDecorationBlockStateId(x, aboveY, z)
        if (!Blocks.isAir(aboveStateId) && !Blocks.isVineState(aboveStateId)) return@repeat
        val supportStateId = chunk.getDecorationBlockStateId(x, y, z)
        if (!BlockShapes.isSolidRender(supportStateId)) return@repeat
        if (decorationHeightAt(chunk, x, z, Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES) >
            aboveY
        ) {
            return@repeat
        }
        val choice = random.nextInt(decorator.maxSegmentAmount * 4)
        val segmentOffset = choice / 4
        val facingOffset = LEAF_LITTER_FACING_STATE_OFFSETS[choice % 4]
        setBlock(chunk, x, aboveY, z, defaultLeafLitterStateId + facingOffset + segmentOffset)
    }
}

/** // parity: BeehiveDecorator placement, occupant data, and RNG. */
internal fun TreeFeature.decorateBeehive(
    chunk: Chunk,
    random: WorldgenRandom,
    context: TreeDecoratorContext,
    probability: Float,
) {
    if (context.logs.isEmpty() || random.nextFloat() >= probability) return
    val hiveY =
        if (context.leaves.isNotEmpty()) {
            max(context.leaves.first().y - 1, context.logs.first().y + 1)
        } else {
            min(context.logs.first().y + 1 + random.nextInt(3), context.logs.last().y)
        }
    val placements = mutableListOf<BlockPos>()
    for (log in context.logs) {
        if (log.y != hiveY) continue
        for ((dx, dz) in BEEHIVE_SPAWN_DIRECTIONS) placements += BlockPos(log.x + dx, hiveY, log.z + dz)
    }
    if (placements.isEmpty()) return
    shuffle(placements, random)
    val hive =
        placements.firstOrNull { isAir(chunk, it.x, it.y, it.z) && isAir(chunk, it.x, it.y, it.z + 1) } ?: return
    setBlock(chunk, hive.x, hive.y, hive.z, southEmptyBeeNestStateId)
    val beeCount = 2 + random.nextInt(2)
    val bees = mutableListOf<NbtTag>()
    repeat(beeCount) {
        val entityData = NbtCompound()
        entityData.putString("id", "minecraft:bee")
        val occupant = NbtCompound()
        occupant["entity_data"] = entityData
        occupant.putInt("ticks_in_hive", random.nextInt(599))
        occupant.putInt("min_ticks_in_hive", 600)
        bees += occupant
    }
    val data = NbtCompound()
    data["bees"] = NbtList(NbtType.COMPOUND, bees)
    chunk.setDecorationBlockEntity(
        hive.x,
        hive.y,
        hive.z,
        southEmptyBeeNestStateId,
        Identifier.vanilla("beehive"),
        data,
    )
}

internal fun TreeFeature.lowestTrunkOrRootPositions(context: TreeDecoratorContext): List<BlockPos> =
    when {
        context.roots.isEmpty() -> context.logs
        context.logs.isNotEmpty() && context.roots.first().y == context.logs.first().y ->
            context.logs +
                context.roots
        else -> context.roots
    }

internal fun <T> TreeFeature.shuffle(
    values: MutableList<T>,
    random: WorldgenRandom,
) {
    for (size in values.size downTo 2) {
        val swapTo = random.nextInt(size)
        val last = values[size - 1]
        values[size - 1] = values[swapTo]
        values[swapTo] = last
    }
}

internal fun TreeFeature.addHangingVine(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
    faceBit: Int,
) {
    val stateId = defaultVineStateId - faceBit
    setBlock(chunk, x, y, z, stateId)
    var cursorY = y - 1
    var remaining = 4
    while (remaining > 0 && isAir(chunk, x, cursorY, z)) {
        setBlock(chunk, x, cursorY, z, stateId)
        cursorY--
        remaining--
    }
}

internal fun TreeFeature.placePodzolCircle(
    chunk: Chunk,
    centerX: Int,
    centerY: Int,
    centerZ: Int,
) {
    for (dx in -2..2) {
        for (dz in -2..2) {
            if (abs(dx) != 2 || abs(dz) != 2) placePodzolAt(chunk, centerX + dx, centerY, centerZ + dz)
        }
    }
}

internal fun TreeFeature.placePodzolAt(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
) {
    for (dy in 2 downTo -3) {
        val cursorY = y + dy
        if (cursorY !in WorldHeight.MIN_Y..WorldHeight.MAX_Y) continue
        val stateId = chunk.getDecorationBlockStateId(x, cursorY, z)
        if (Blocks.byStateId(stateId)?.block?.path in beneathTreePodzolReplaceablePaths) {
            chunk.setDecorationBlockStateId(x, cursorY, z, Blocks.PODZOL.stateId)
            return
        }
        if (!Blocks.isAir(stateId) && dy < 0) return
    }
}

internal fun TreeFeature.isAir(
    chunk: Chunk,
    x: Int,
    y: Int,
    z: Int,
): Boolean =
    y in WorldHeight.MIN_Y..WorldHeight.MAX_Y &&
        Blocks.isAir(chunk.getDecorationBlockStateId(x, y, z))

private data class VinePlacement(
    val dx: Int,
    val dz: Int,
    val faceBit: Int,
)

private data class CocoaPlacement(
    val dx: Int,
    val dz: Int,
    val facingOffset: Int,
)

private val beneathTreePodzolReplaceablePaths: Set<String> =
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
    )

private val TRUNK_VINES =
    listOf(
        VinePlacement(-1, 0, 16),
        VinePlacement(1, 0, 1),
        VinePlacement(0, -1, 4),
        VinePlacement(0, 1, 8),
    )
private val defaultVineStateId =
    Blocks.REGISTRY
        .byIdentifier(
            org.basaltmc.wire.common.Identifier
                .vanilla("vine"),
        )?.stateId
        ?: error("unknown block 'vine'")
private val COCOA_DIRECTIONS =
    listOf(
        CocoaPlacement(0, 1, 0),
        CocoaPlacement(-1, 0, 3),
        CocoaPlacement(0, -1, 1),
        CocoaPlacement(1, 0, 2),
    )
private val defaultCocoaStateId =
    Blocks.REGISTRY
        .byIdentifier(
            org.basaltmc.wire.common.Identifier
                .vanilla("cocoa"),
        )?.stateId
        ?: error("unknown block 'cocoa'")
private val paleHangingMossTipTrueStateId =
    Blocks.REGISTRY
        .byIdentifier(
            org.basaltmc.wire.common.Identifier
                .vanilla("pale_hanging_moss"),
        )?.stateId
        ?: error("unknown block 'pale_hanging_moss'")
private val paleHangingMossTipFalseStateId = paleHangingMossTipTrueStateId + 1
private val naturalDormantCreakingHeartStateId =
    (
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("creaking_heart"),
            )?.stateId
            ?: error("unknown block 'creaking_heart'")
    ) + 1
private val hangingMangrovePropaguleAge0StateId =
    (
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("mangrove_propagule"),
            )?.stateId
            ?: error("unknown block 'mangrove_propagule'")
    ) - 4
private val defaultLeafLitterStateId =
    Blocks.REGISTRY
        .byIdentifier(
            org.basaltmc.wire.common.Identifier
                .vanilla("leaf_litter"),
        )?.stateId
        ?: error("unknown block 'leaf_litter'")
private val southEmptyBeeNestStateId =
    (
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla("bee_nest"),
            )?.stateId
            ?: error("unknown block 'bee_nest'")
    ) + 6
private val BEEHIVE_SPAWN_DIRECTIONS = listOf(1 to 0, 0 to 1, -1 to 0)
private val LEAF_LITTER_FACING_STATE_OFFSETS = intArrayOf(0, 12, 4, 8)
