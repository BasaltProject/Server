package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.hasFluid
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound

/**
 * // parity: the four `postProcess` bodies of MineshaftPieces, which turn an assembled shaft into
 * blocks. Several rolls are conditional on the terrain, so the shaft needs seed-exact ground first.
 */
public object MineshaftFill {
    /** Writes whatever of [piece] falls inside [clip], which is one chunk's share of it. */
    public fun place(
        piece: MineshaftPiece,
        blocks: MineshaftBlocks,
        chunk: Chunk,
        clip: StructureBoundingBox,
        random: BasaltRandom,
        oceanFloorHeight: (x: Int, z: Int) -> Int,
    ) {
        val fill =
            StructurePieceFill(
                box = piece.box,
                orientation = piece.fillOrientation,
                chunk = chunk,
                clip = clip,
                oceanFloorHeight = oceanFloorHeight,
                canBeReplaced = { stateId -> canBeReplaced(stateId, blocks) },
            )
        if (isInInvalidLocation(piece, chunk, clip)) return
        when (piece.kind) {
            MineshaftPiece.Kind.CORRIDOR -> corridor(piece, blocks, fill, random)
            MineshaftPiece.Kind.CROSSING -> crossing(piece, blocks, fill)
            MineshaftPiece.Kind.STAIRS -> stairs(fill)
            MineshaftPiece.Kind.ROOM -> room(piece, fill)
        }
    }

    private fun corridor(
        piece: MineshaftPiece,
        blocks: MineshaftBlocks,
        fill: StructurePieceFill,
        random: BasaltRandom,
    ) {
        val length = piece.sections * 5 - 1
        val caveAir = Blocks.CAVE_AIR.stateId
        fill.generateBox(0, 0, 0, 2, 1, length, caveAir, caveAir, skipAir = false)
        fill.generateMaybeBox(
            random,
            0.8f,
            0,
            2,
            0,
            2,
            2,
            length,
            caveAir,
            caveAir,
            skipAir = false,
            onlyInside = false,
        )
        if (piece.spiderCorridor) {
            fill.generateMaybeBox(
                random,
                0.6f,
                0,
                0,
                0,
                2,
                1,
                length,
                COBWEB,
                caveAir,
                skipAir = false,
                onlyInside = true,
            )
        }

        var placedSpider = false
        for (section in 0 until piece.sections) {
            val z = 2 + section * 5
            placeSupport(fill, blocks, random, z)
            maybeCobweb(fill, random, 0.1f, 0, 2, z - 1)
            maybeCobweb(fill, random, 0.1f, 2, 2, z - 1)
            maybeCobweb(fill, random, 0.1f, 0, 2, z + 1)
            maybeCobweb(fill, random, 0.1f, 2, 2, z + 1)
            maybeCobweb(fill, random, 0.05f, 0, 2, z - 2)
            maybeCobweb(fill, random, 0.05f, 2, 2, z - 2)
            maybeCobweb(fill, random, 0.05f, 0, 2, z + 2)
            maybeCobweb(fill, random, 0.05f, 2, 2, z + 2)
            if (random.nextInt(100) == 0) placeChestRail(fill, random, 2, 0, z - 1)
            if (random.nextInt(100) == 0) placeChestRail(fill, random, 0, 0, z + 1)
            if (piece.spiderCorridor && !placedSpider) {
                val spawnerZ = z - 1 + random.nextInt(3)
                // The spawner block would be a broken one without its block entity, so it is left
                // out; the draws and the flag still have to happen or the shaft shifts from here on.
                if (fill.isInterior(1, 0, spawnerZ)) placedSpider = true
            }
        }

        for (x in 0..2) {
            for (z in 0..length) setPlanks(fill, blocks, x, -1, z)
        }

        placeDoubleSupport(fill, blocks, 0, -1, 2)
        if (piece.sections > 1) placeDoubleSupport(fill, blocks, 0, -1, length - 2)

        if (!piece.hasRails) return
        for (z in 0..length) {
            val floor = fill.blockAt(1, -1, z)
            if (Blocks.isAir(floor) || !BlockShapes.isSolidRender(floor)) continue
            val probability = if (fill.isInterior(1, 0, z)) 0.7f else 0.9f
            fill.maybePlace(random, probability, 1, 0, z, RAIL_NORTH_SOUTH)
        }
    }

    /** // parity: MineShaftCrossing.postProcess, written in world coordinates like vanilla's. */
    private fun crossing(
        piece: MineshaftPiece,
        blocks: MineshaftBlocks,
        fill: StructurePieceFill,
    ) {
        val box = piece.box
        val caveAir = Blocks.CAVE_AIR.stateId

        fun box3(
            x0: Int,
            y0: Int,
            z0: Int,
            x1: Int,
            y1: Int,
            z1: Int,
        ) = fill.generateBox(x0, y0, z0, x1, y1, z1, caveAir, caveAir, skipAir = false)

        if (piece.twoFloored) {
            val lower = box.minY + LOWER_FLOOR_HEIGHT - 1
            box3(box.minX + 1, box.minY, box.minZ, box.maxX - 1, lower, box.maxZ)
            box3(box.minX, box.minY, box.minZ + 1, box.maxX, lower, box.maxZ - 1)
            box3(box.minX + 1, box.maxY - 2, box.minZ, box.maxX - 1, box.maxY, box.maxZ)
            box3(box.minX, box.maxY - 2, box.minZ + 1, box.maxX, box.maxY, box.maxZ - 1)
            val landing = box.minY + LOWER_FLOOR_HEIGHT
            box3(box.minX + 1, landing, box.minZ + 1, box.maxX - 1, landing, box.maxZ - 1)
        } else {
            box3(box.minX + 1, box.minY, box.minZ, box.maxX - 1, box.maxY, box.maxZ)
            box3(box.minX, box.minY, box.minZ + 1, box.maxX, box.maxY, box.maxZ - 1)
        }

        supportPillar(fill, blocks, box.minX + 1, box.minY, box.minZ + 1, box.maxY)
        supportPillar(fill, blocks, box.minX + 1, box.minY, box.maxZ - 1, box.maxY)
        supportPillar(fill, blocks, box.maxX - 1, box.minY, box.minZ + 1, box.maxY)
        supportPillar(fill, blocks, box.maxX - 1, box.minY, box.maxZ - 1, box.maxY)
        for (x in box.minX..box.maxX) {
            for (z in box.minZ..box.maxZ) setPlanks(fill, blocks, x, box.minY - 1, z)
        }
    }

    /** A corner post, but only where the crossing has a ceiling for it to hold up. */
    private fun supportPillar(
        fill: StructurePieceFill,
        blocks: MineshaftBlocks,
        x: Int,
        y0: Int,
        z: Int,
        y1: Int,
    ) {
        if (Blocks.isAir(fill.blockAt(x, y1 + 1, z))) return
        fill.generateBox(x, y0, z, x, y1, z, blocks.planks, Blocks.CAVE_AIR.stateId, skipAir = false)
    }

    /** // parity: MineShaftStairs.postProcess, a landing at each end and five carved steps. */
    private fun stairs(fill: StructurePieceFill) {
        val caveAir = Blocks.CAVE_AIR.stateId
        fill.generateBox(0, 5, 0, 2, 7, 1, caveAir, caveAir, skipAir = false)
        fill.generateBox(0, 0, 7, 2, 2, 8, caveAir, caveAir, skipAir = false)
        for (step in 0 until 5) {
            val bottom = 5 - step - (if (step < 4) 1 else 0)
            fill.generateBox(0, bottom, 2 + step, 2, 7 - step, 2 + step, caveAir, caveAir, skipAir = false)
        }
    }

    /** // parity: MineShaftRoom.postProcess: a hollow floor, a mouth per passage, and a dome. */
    private fun room(
        piece: MineshaftPiece,
        fill: StructurePieceFill,
    ) {
        val box = piece.box
        val caveAir = Blocks.CAVE_AIR.stateId
        fill.generateBox(
            box.minX,
            box.minY + 1,
            box.minZ,
            box.maxX,
            minOf(box.minY + 3, box.maxY),
            box.maxZ,
            caveAir,
            caveAir,
            skipAir = false,
        )
        for (entrance in piece.entrances) {
            fill.generateBox(
                entrance.minX,
                entrance.maxY - 2,
                entrance.minZ,
                entrance.maxX,
                entrance.maxY,
                entrance.maxZ,
                caveAir,
                caveAir,
                skipAir = false,
            )
        }
        fill.generateUpperHalfSphere(
            box.minX,
            box.minY + 4,
            box.minZ,
            box.maxX,
            box.maxY,
            box.maxZ,
            caveAir,
            skipAir = false,
        )
    }

    /**
     * // parity: MineShaftCorridor.createChest: a rail with a random axis, and a chest minecart
     * riding it with the shaft's loot table and its own seed.
     */
    private fun placeChestRail(
        fill: StructurePieceFill,
        random: BasaltRandom,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (!Blocks.isAir(fill.blockAt(x, y, z))) return
        if (Blocks.isAir(fill.blockAt(x, y - 1, z))) return
        val shape = if (random.nextBoolean()) RAIL_NORTH_SOUTH else RAIL_EAST_WEST
        fill.place(shape, x, y, z)
        val lootSeed = random.nextLong()
        fill.placeEntity(
            Identifier.vanilla("chest_minecart"),
            x,
            y,
            z,
            NbtCompound().apply {
                putString("LootTable", "minecraft:chests/abandoned_mineshaft")
                putLong("LootTableSeed", lootSeed)
            },
        )
    }

    /** A piece never overwrites another piece's frame. // parity: MineShaftPiece.canBeReplaced. */
    private fun canBeReplaced(
        stateId: Int,
        blocks: MineshaftBlocks,
    ): Boolean {
        val block = Blocks.byStateId(stateId)?.block ?: return true
        return block != Blocks.byStateId(blocks.planks)?.block &&
            block != Blocks.byStateId(blocks.log)?.block &&
            block != Blocks.byStateId(blocks.fence)?.block &&
            block.path != "iron_chain"
    }

    /**
     * // parity: MineShaftPiece.isInInvalidLocation. A shaft refuses to open into liquid, checking
     * the shell of its own box one block out, clipped to what this chunk can see.
     */
    private fun isInInvalidLocation(
        piece: MineshaftPiece,
        chunk: Chunk,
        clip: StructureBoundingBox,
    ): Boolean {
        val x0 = maxOf(piece.box.minX - 1, clip.minX)
        val y0 = maxOf(piece.box.minY - 1, clip.minY)
        val z0 = maxOf(piece.box.minZ - 1, clip.minZ)
        val x1 = minOf(piece.box.maxX + 1, clip.maxX)
        val y1 = minOf(piece.box.maxY + 1, clip.maxY)
        val z1 = minOf(piece.box.maxZ + 1, clip.maxZ)

        fun liquid(
            x: Int,
            y: Int,
            z: Int,
        ) = Blocks.hasFluid(chunk.getDecorationBlockStateId(x, y, z))

        for (x in x0..x1) {
            for (z in z0..z1) {
                if (liquid(x, y0, z) || liquid(x, y1, z)) return true
            }
        }
        for (x in x0..x1) {
            for (y in y0..y1) {
                if (liquid(x, y, z0) || liquid(x, y, z1)) return true
            }
        }
        for (z in z0..z1) {
            for (y in y0..y1) {
                if (liquid(x0, y, z) || liquid(x1, y, z)) return true
            }
        }
        return false
    }

    /** // parity: placeSupport. Two fence posts and a beam, or two stubs, once every five blocks. */
    private fun placeSupport(
        fill: StructurePieceFill,
        blocks: MineshaftBlocks,
        random: BasaltRandom,
        z: Int,
    ) {
        if (!isSupportingBox(fill, z)) return
        val caveAir = Blocks.CAVE_AIR.stateId
        fill.generateBox(0, 0, z, 0, 1, z, blocks.fenceWest, caveAir, skipAir = false)
        fill.generateBox(2, 0, z, 2, 1, z, blocks.fenceEast, caveAir, skipAir = false)
        if (random.nextInt(4) == 0) {
            fill.generateBox(0, 2, z, 0, 2, z, blocks.planks, caveAir, skipAir = false)
            fill.generateBox(2, 2, z, 2, 2, z, blocks.planks, caveAir, skipAir = false)
            return
        }
        fill.generateBox(0, 2, z, 2, 2, z, blocks.planks, caveAir, skipAir = false)
        fill.maybePlace(random, 0.05f, 1, 2, z - 1, WALL_TORCH_SOUTH)
        fill.maybePlace(random, 0.05f, 1, 2, z + 1, WALL_TORCH_NORTH)
    }

    /** Whether the ceiling above the support line is closed, so a beam has something to bridge. */
    private fun isSupportingBox(
        fill: StructurePieceFill,
        z: Int,
    ): Boolean {
        for (x in 0..2) {
            if (Blocks.isAir(fill.blockAt(x, 3, z))) return false
        }
        return true
    }

    /** // parity: maybePlaceCobWeb, whose roll only happens where the spot is already underground. */
    private fun maybeCobweb(
        fill: StructurePieceFill,
        random: BasaltRandom,
        probability: Float,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (!fill.isInterior(x, y, z)) return
        if (random.nextFloat() >= probability) return
        if (!hasSturdyNeighbours(fill, x, y, z)) return
        fill.place(COBWEB, x, y, z)
    }

    private fun hasSturdyNeighbours(
        fill: StructurePieceFill,
        x: Int,
        y: Int,
        z: Int,
    ): Boolean {
        var sturdy = 0
        for (face in BlockFace.entries) {
            val neighbour = fill.blockAt(x + face.stepX, y + face.stepY, z + face.stepZ)
            if (BlockShapes.isFaceSturdy(neighbour, face.opposite)) {
                sturdy++
                if (sturdy >= 2) return true
            }
        }
        return false
    }

    /** // parity: setPlanksBlock, which floors the corridor only where it has no footing already. */
    private fun setPlanks(
        fill: StructurePieceFill,
        blocks: MineshaftBlocks,
        x: Int,
        y: Int,
        z: Int,
    ) {
        if (!fill.isInterior(x, y, z)) return
        if (BlockShapes.isFaceSturdy(fill.blockAt(x, y, z), BlockFace.UP)) return
        fill.place(blocks.planks, x, y, z)
    }

    private fun placeDoubleSupport(
        fill: StructurePieceFill,
        blocks: MineshaftBlocks,
        x: Int,
        y: Int,
        z: Int,
    ) {
        val planksBlock = Blocks.byStateId(blocks.planks)?.block
        if (Blocks.byStateId(fill.blockAt(x, y, z))?.block == planksBlock) {
            fill.place(blocks.log, x, y, z)
        }
        if (Blocks.byStateId(fill.blockAt(x + 2, y, z))?.block == planksBlock) {
            fill.place(blocks.log, x + 2, y, z)
        }
    }

    private fun state(
        path: String,
        properties: Map<String, String>,
    ): Int = Blocks.stateIdOf(Identifier.vanilla(path), properties) ?: error("unknown state '$path$properties'")

    private const val LOWER_FLOOR_HEIGHT = 3

    private val COBWEB =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("cobweb"))?.stateId ?: error("unknown block 'cobweb'")
    private val RAIL_NORTH_SOUTH = state("rail", mapOf("shape" to "north_south", "waterlogged" to "false"))
    private val RAIL_EAST_WEST = state("rail", mapOf("shape" to "east_west", "waterlogged" to "false"))
    private val WALL_TORCH_SOUTH = state("wall_torch", mapOf("facing" to "south"))
    private val WALL_TORCH_NORTH = state("wall_torch", mapOf("facing" to "north"))
}
