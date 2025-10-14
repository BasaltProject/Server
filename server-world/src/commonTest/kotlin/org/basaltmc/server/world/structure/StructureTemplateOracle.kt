package org.basaltmc.server.world.structure

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/**
 * The frozen Kotlin as the oracle for the structure template. The template is built here rather
 * than loaded from a file: no Mojang asset goes anywhere near the repository, and a synthetic one
 * can carry every property family the transform touches at once.
 */
class StructureTemplateOracle {
    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun intList(vararg values: Int) = NbtList(NbtType.INT, values.map { NbtInt(it) })

    private fun paletteEntry(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): NbtCompound {
        val entry = NbtCompound()
        entry.putString("Name", name)
        if (properties.isNotEmpty()) {
            val tag = NbtCompound()
            for ((key, value) in properties) tag.putString(key, value)
            entry.putCompound("Properties", tag)
        }
        return entry
    }

    private fun blockEntry(
        x: Int,
        y: Int,
        z: Int,
        state: Int,
        nbt: NbtCompound? = null,
    ): NbtCompound {
        val entry = NbtCompound()
        entry["pos"] = intList(x, y, z)
        entry.putInt("state", state)
        if (nbt != null) entry.putCompound("nbt", nbt)
        return entry
    }

    /** Every property family the transform touches, in one template. */
    private fun templateNbt(palettes: Int = 1): NbtCompound {
        val palette =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    paletteEntry("minecraft:stone"),
                    paletteEntry(
                        "minecraft:oak_stairs",
                        mapOf(
                            "facing" to "north",
                            "half" to "bottom",
                            "shape" to "outer_left",
                            "waterlogged" to "false",
                        ),
                    ),
                    paletteEntry("minecraft:rail", mapOf("shape" to "north_east", "waterlogged" to "false")),
                    paletteEntry(
                        "minecraft:oak_door",
                        mapOf(
                            "facing" to "east",
                            "half" to "lower",
                            "hinge" to "left",
                            "open" to "false",
                            "powered" to "false",
                        ),
                    ),
                    paletteEntry(
                        "minecraft:oak_fence",
                        mapOf(
                            "north" to "true",
                            "east" to "false",
                            "south" to "false",
                            "west" to "true",
                            "waterlogged" to "false",
                        ),
                    ),
                    paletteEntry("minecraft:oak_sign", mapOf("rotation" to "3", "waterlogged" to "false")),
                    paletteEntry("minecraft:jigsaw", mapOf("orientation" to "north_up")),
                    paletteEntry(
                        "minecraft:chest",
                        mapOf(
                            "facing" to "west",
                            "type" to "single",
                            "waterlogged" to "false",
                        ),
                    ),
                    paletteEntry("minecraft:structure_block", mapOf("mode" to "data")),
                    paletteEntry("minecraft:oak_log", mapOf("axis" to "x")),
                    // A second sign past the halfway mark: the rotation property is read as signed,
                    // and anything at or below 8 never notices.
                    paletteEntry("minecraft:oak_sign", mapOf("rotation" to "13", "waterlogged" to "false")),
                    // An inner corner stair: only a left-right mirror swaps those halves, and the
                    // outer corner above cannot show that.
                    paletteEntry(
                        "minecraft:oak_stairs",
                        mapOf("facing" to "south", "half" to "top", "shape" to "inner_left", "waterlogged" to "false"),
                    ),
                    // The same inner corner facing east: a front-back mirror crosses that axis and a
                    // left-right one does not, which is where the two halves stop agreeing.
                    paletteEntry(
                        "minecraft:oak_stairs",
                        mapOf("facing" to "east", "half" to "top", "shape" to "inner_left", "waterlogged" to "false"),
                    ),
                ),
            )
        val jigsawNbt = NbtCompound()
        jigsawNbt.putString("id", "minecraft:jigsaw")
        jigsawNbt.putString("joint", "aligned")
        jigsawNbt.putString("name", "minecraft:test_name")
        jigsawNbt.putString("pool", "minecraft:test_pool")
        jigsawNbt.putString("target", "minecraft:test_target")
        jigsawNbt.putString("final_state", "minecraft:oak_planks")
        jigsawNbt.putInt("placement_priority", 3)
        jigsawNbt.putInt("selection_priority", 7)
        val chestNbt = NbtCompound()
        chestNbt.putString("id", "minecraft:chest")
        val markerNbt = NbtCompound()
        markerNbt.putString("id", "minecraft:structure_block")
        markerNbt.putString("mode", "DATA")
        markerNbt.putString("metadata", "chest_east")

        val blocks =
            NbtList(
                NbtType.COMPOUND,
                listOf(
                    blockEntry(0, 0, 0, 0),
                    blockEntry(1, 0, 0, 1),
                    blockEntry(2, 0, 0, 2),
                    blockEntry(0, 1, 0, 3),
                    blockEntry(1, 1, 0, 4),
                    blockEntry(2, 1, 0, 5),
                    blockEntry(0, 0, 1, 6, jigsawNbt),
                    blockEntry(1, 0, 1, 7, chestNbt),
                    blockEntry(2, 0, 1, 8, markerNbt),
                    blockEntry(0, 1, 1, 9),
                    blockEntry(1, 1, 1, 10),
                    blockEntry(2, 1, 1, 11),
                    blockEntry(0, 0, 2, 12),
                    blockEntry(1, 0, 2, 2),
                    blockEntry(2, 0, 2, 4),
                ),
            )

        val entityNbt = NbtCompound()
        entityNbt.putString("id", "minecraft:armor_stand")
        entityNbt["Rotation"] = NbtList(NbtType.FLOAT, listOf(NbtFloat(45f), NbtFloat(10f)))
        val entity = NbtCompound()
        entity["pos"] = NbtList(NbtType.DOUBLE, listOf(NbtDouble(1.5), NbtDouble(1.0), NbtDouble(0.5)))
        entity["blockPos"] = intList(1, 1, 0)
        entity.putCompound("nbt", entityNbt)

        val nbt = NbtCompound()
        nbt["size"] = intList(3, 2, 3)
        nbt["blocks"] = blocks
        if (palettes > 1) {
            // A second palette that differs only in its first state, which is what the draw picks.
            val other =
                NbtList(
                    NbtType.COMPOUND,
                    palette.toList().mapIndexed { index, entry ->
                        if (index == 0) paletteEntry("minecraft:cobblestone") else entry
                    },
                )
            nbt["palettes"] = NbtList(NbtType.LIST, listOf(palette, other))
        } else {
            nbt["palette"] = palette
        }
        nbt["entities"] = NbtList(NbtType.COMPOUND, listOf(entity))
        return nbt
    }

    private fun loaded(palettes: Int = 1): StructureTemplate {
        val template = StructureTemplate()
        template.load(templateNbt(palettes))
        return template
    }

    private fun groundChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 60..64) chunk.setBlockStateId(x, y, z, Blocks.STONE.stateId)
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var placed = 0
        for (y in 58..80) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != Blocks.AIR.stateId && id != Blocks.STONE.stateId) placed++
                }
            }
        }
        // The entities travel with the placement: their transformed position and yaw are the only
        // observable the block fold would otherwise miss.
        for (entity in chunk.generatedEntities()) {
            checksum = (checksum xor entity.x.toRawBits()) * 1099511628211L
            checksum = (checksum xor entity.y.toRawBits()) * 1099511628211L
            checksum = (checksum xor entity.z.toRawBits()) * 1099511628211L
            checksum = (checksum xor entity.yaw.toRawBits().toLong()) * 1099511628211L
            checksum = (checksum xor entity.pitch.toRawBits().toLong()) * 1099511628211L
        }
        return Triple(checksum, placed, chunk.blockEntities().size)
    }

    @Test
    fun dumpLoad() {
        val template = loaded()
        println(
            "ORACLE load size=${template.size.x},${template.size.y},${template.size.z} " +
                "blocks=${template.blocks.size} entities=${template.entities.size} palettes=${template.paletteCount}",
        )
        val many = loaded(palettes = 2)
        println("ORACLE loadPalettes palettes=${many.paletteCount} blocks=${many.blocks.size}")
        val states = template.blocks.joinToString(",") { "${it.pos.x}${it.pos.y}${it.pos.z}:${it.stateId}" }
        println("ORACLE states rows=$states")
    }

    @Test
    fun dumpTransforms() {
        val template = loaded()
        for (mirror in StructureMirror.entries) {
            for (rotation in StructureRotation.entries) {
                val rows =
                    template.blocks.joinToString(",") {
                        template.transformedStateId(it.stateId, it.properties, mirror, rotation).toString()
                    }
                println("ORACLE transform_${mirror.name}_${rotation.name} rows=$rows")
            }
        }
    }

    @Test
    fun dumpQueries() {
        val template = loaded()
        for (rotation in StructureRotation.entries) {
            val box =
                template.getBoundingBox(
                    StructurePos(10, 64, 20),
                    rotation,
                    StructurePos(0, 0, 0),
                    StructureMirror.NONE,
                )
            val jigsaws =
                template.jigsawBlocks(StructurePos(10, 64, 20), rotation, StructureMirror.NONE, StructurePos(0, 0, 0))
            val markers =
                template.dataMarkers(StructurePos(10, 64, 20), rotation, StructureMirror.NONE, StructurePos(0, 0, 0))
            println(
                "ORACLE query_${rotation.name} " +
                    "box=${box.minX},${box.minY},${box.minZ},${box.maxX},${box.maxY},${box.maxZ} " +
                    "jigsaws=${jigsaws.size} jigsaw=${jigsaws.firstOrNull()?.let {
                        "${it.pos.x},${it.pos.y},${it.pos.z}/${it.front.name}/${it.top.name}/${it.rollable}/" +
                            "${it.name}/${it.pool}/${it.target}/${it.finalStateName}/${it.placementPriority}/" +
                            it.selectionPriority
                    } ?: "-"} " +
                    "markers=${markers.size} marker=${markers.firstOrNull()?.let {
                        "${it.pos.x},${it.pos.y},${it.pos.z}/${it.metadata}"
                    } ?: "-"}",
            )
        }
    }

    @Test
    fun dumpPlacement() {
        for (mirror in StructureMirror.entries) {
            for (rotation in StructureRotation.entries) {
                val template = loaded()
                val chunk = groundChunk(0, 0)
                val random = WorldgenRandom(LegacyRandom(5309L))
                val position = StructurePos(4, 65, 6)
                val clip = StructureBoundingBox(0, -64, 0, 15, 319, 15)
                template.placeInto(
                    chunk = chunk,
                    position = position,
                    rotation = rotation,
                    mirror = mirror,
                    pivot = StructurePos(0, 0, 0),
                    clipTo = clip,
                    random = random,
                )
                val (checksum, placed, blockEntities) = fold(chunk)
                println(
                    "ORACLE place_${mirror.name}_${rotation.name} placed=$placed " +
                        "entities=${chunk.generatedEntities().size} " +
                        "blockEntities=$blockEntities deferred=${chunk.takeGeneratedBlockWrites().size} " +
                        "checksum=$checksum after=${random.nextInt()}",
                )
            }
        }

        // Terrain matching, which re-reads the frozen surface for every column.
        run {
            val template = loaded()
            val chunk = groundChunk(0, 0)
            val random = WorldgenRandom(LegacyRandom(313L))
            template.placeInto(
                chunk = chunk,
                position = StructurePos(6, 70, 6),
                rotation = StructureRotation.CLOCKWISE_90,
                mirror = StructureMirror.NONE,
                pivot = StructurePos(0, 0, 0),
                clipTo = StructureBoundingBox(0, -64, 0, 15, 319, 15),
                random = random,
                gravityOffset = -1,
            )
            val (checksum, placed, blockEntities) = fold(chunk)
            println(
                "ORACLE gravity placed=$placed blockEntities=$blockEntities " +
                    "deferred=${chunk.takeGeneratedBlockWrites().size} checksum=$checksum after=${random.nextInt()}",
            )
        }

        // A clip wider than the chunk, as the fossil has: what falls outside is deferred.
        run {
            val template = loaded()
            val chunk = groundChunk(0, 0)
            val random = WorldgenRandom(LegacyRandom(727L))
            template.placeInto(
                chunk = chunk,
                position = StructurePos(14, 65, 14),
                rotation = StructureRotation.NONE,
                mirror = StructureMirror.NONE,
                pivot = StructurePos(0, 0, 0),
                clipTo = StructureBoundingBox(-16, -64, -16, 31, 319, 31),
                random = random,
            )
            val (checksum, placed, blockEntities) = fold(chunk)
            println(
                "ORACLE wideClip placed=$placed blockEntities=$blockEntities " +
                    "deferred=${chunk.takeGeneratedBlockWrites().size} checksum=$checksum after=${random.nextInt()}",
            )
        }

        // The decoration entry point, which defers whatever crosses the chunk border.
        val template = loaded()
        val chunk = groundChunk(0, 0)
        val random = WorldgenRandom(LegacyRandom(881L))
        val ok = template.placeDecorationInto(chunk, StructurePos(14, 66, 15), StructureRotation.CLOCKWISE_90, random)
        val (checksum, placed, blockEntities) = fold(chunk)
        println(
            "ORACLE decoration ok=$ok placed=$placed blockEntities=$blockEntities " +
                "deferred=${chunk.takeGeneratedBlockWrites().size} checksum=$checksum after=${random.nextInt()}",
        )

        val empty = StructureTemplate()
        println(
            "ORACLE decorationEmpty ok=${empty.placeDecorationInto(
                groundChunk(0, 0),
                StructurePos(0, 64, 0),
                StructureRotation.NONE,
                WorldgenRandom(LegacyRandom(1L)),
            )}",
        )
    }
}
