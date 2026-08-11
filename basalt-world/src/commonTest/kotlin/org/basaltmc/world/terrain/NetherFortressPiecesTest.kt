package org.basaltmc.world.terrain

import org.basaltmc.common.Identifier
import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.structure.StructureBoundingBox
import org.basaltmc.world.structure.StructureDirection
import kotlin.test.Test
import kotlin.test.assertEquals

class NetherFortressPiecesTest {
    @Test
    fun castleTurnChestsMatchOfficialFacingLootAndRandomVectors() {
        val cases =
            buildList {
                for (orientation in listOf(
                    StructureDirection.NORTH,
                    StructureDirection.SOUTH,
                    StructureDirection.WEST,
                    StructureDirection.EAST,
                )) {
                    add(
                        Triple(
                            NetherFortressPieces.PieceType.CASTLE_LEFT_TURN,
                            orientation,
                            if (orientation ==
                                StructureDirection.NORTH
                            ) {
                                "south"
                            } else {
                                "north"
                            },
                        ),
                    )
                    add(
                        Triple(
                            NetherFortressPieces.PieceType.CASTLE_RIGHT_TURN,
                            orientation,
                            if (orientation ==
                                StructureDirection.SOUTH
                            ) {
                                "north"
                            } else {
                                "south"
                            },
                        ),
                    )
                }
            }
        for ((type, orientation, expectedFacing) in cases) {
            val piece =
                NetherFortressPieces.Piece(
                    type,
                    0,
                    StructureBoundingBox(0, 20, 0, 4, 26, 4),
                    orientation,
                    needsChest = true,
                )
            val chunk = Chunk(0, 0, ChunkHeight.NETHER)
            val random = WorldgenRandom(XoroshiroRandom(91L))
            val oracle = WorldgenRandom(XoroshiroRandom(91L))
            val expectedLootSeed = oracle.nextLong()
            assertEquals(-8253827800390094055L, expectedLootSeed)

            piece.place(chunk, clip(chunk), random)

            val chest = chunk.blockEntities().single()
            assertEquals(Identifier.vanilla("chest"), chest.type, "$type $orientation")
            assertEquals("minecraft:chests/nether_bridge", chest.data.getString("LootTable"))
            assertEquals(expectedLootSeed, chest.data.getLong("LootTableSeed"))
            assertEquals(0, chest.data.getCompound("components").size)
            assertEquals(
                expectedFacing,
                Blocks.byStateId(chunk.getBlockStateId(chest.localX, chest.y, chest.localZ))?.properties?.get("facing"),
            )
            assertEquals(6715030100007076810L, oracle.nextLong())
            assertEquals(6715030100007076810L, random.nextLong())
        }
    }

    @Test
    fun monsterThroneCreatesABlazeSpawnerWithoutConsumingPlacementRandom() {
        val piece =
            NetherFortressPieces.Piece(
                NetherFortressPieces.PieceType.MONSTER_THRONE,
                0,
                StructureBoundingBox(0, 20, 0, 6, 27, 8),
                StructureDirection.NORTH,
            )
        val chunk = Chunk(0, 0, ChunkHeight.NETHER)
        val random = WorldgenRandom(XoroshiroRandom(91L))
        val oracle = WorldgenRandom(XoroshiroRandom(91L))

        piece.place(chunk, clip(chunk), random)

        val spawner = chunk.blockEntities().single()
        assertEquals(Identifier.vanilla("mob_spawner"), spawner.type)
        assertEquals(20, spawner.data.getShort("Delay"))
        assertEquals(200, spawner.data.getShort("MinSpawnDelay"))
        assertEquals(800, spawner.data.getShort("MaxSpawnDelay"))
        assertEquals(
            "minecraft:blaze",
            spawner.data
                .getCompound("SpawnData")
                .getCompound("entity")
                .getString("id"),
        )
        assertEquals(0, spawner.data.getList("SpawnPotentials")!!.size)
        assertEquals(0, spawner.data.getCompound("components").size)
        assertEquals(-8253827800390094055L, oracle.nextLong())
        assertEquals(-8253827800390094055L, random.nextLong())
    }

    @Test
    fun castleEntranceSchedulesItsImmediateLavaTick() {
        val piece =
            NetherFortressPieces.Piece(
                NetherFortressPieces.PieceType.CASTLE_ENTRANCE,
                0,
                StructureBoundingBox(0, 20, 0, 12, 33, 12),
                StructureDirection.NORTH,
            )
        val chunk = Chunk(0, 0, ChunkHeight.NETHER)

        piece.place(chunk, clip(chunk), WorldgenRandom(XoroshiroRandom(91L)))

        val tick = chunk.scheduledFluidTicks().single()
        assertEquals(Identifier.vanilla("lava"), tick.type)
        assertEquals(6, tick.localX)
        assertEquals(25, tick.y)
        assertEquals(6, tick.localZ)
        assertEquals(0, tick.delay)
        assertEquals(0, tick.priority)
    }

    @Test
    fun fortressDirectionalStatesMatchVanillaReportIds() {
        val fence = Identifier.vanilla("nether_brick_fence")
        val stairs = Identifier.vanilla("nether_brick_stairs")

        assertEquals(
            9346,
            Blocks.stateIdOf(
                fence,
                linkedMapOf(
                    "east" to "true",
                    "north" to "false",
                    "south" to "true",
                    "waterlogged" to "false",
                    "west" to "false",
                ),
            ),
        )
        assertEquals(
            9398,
            Blocks.stateIdOf(
                stairs,
                linkedMapOf(
                    "facing" to "south",
                    "half" to "bottom",
                    "shape" to "straight",
                    "waterlogged" to "false",
                ),
            ),
        )
    }

    @Test
    fun pieceOrientationRotatesFenceAndStairStates() {
        val assemblyRandom = WorldgenRandom(LegacyRandom(0L))
        assemblyRandom.setLargeFeatureSeed(0L, 0, 0)
        val pieces = NetherFortressPieces.assemble(assemblyRandom, 0, 0)
        val eastBridge =
            pieces.first {
                it.type == NetherFortressPieces.PieceType.BRIDGE_STRAIGHT &&
                    it.orientation.name == "EAST"
            }
        val bridgeChunk = Chunk(1, 0, ChunkHeight.NETHER)
        eastBridge.place(bridgeChunk, clip(bridgeChunk), WorldgenRandom(LegacyRandom(0L)))
        val fence = Blocks.byStateId(bridgeChunk.getBlockStateId(22 and 15, 53, 9))

        assertEquals(
            linkedMapOf(
                "east" to "true",
                "north" to "false",
                "south" to "true",
                "waterlogged" to "false",
                "west" to "true",
            ),
            fence?.properties,
        )

        val westStairs = pieces.first { it.type == NetherFortressPieces.PieceType.CASTLE_STAIRS }
        val stairsChunk = Chunk(-1, 2, ChunkHeight.NETHER)
        westStairs.place(stairsChunk, clip(stairsChunk), WorldgenRandom(LegacyRandom(0L)))
        val stairs = Blocks.byStateId(stairsChunk.getBlockStateId(-11 and 15, 56, 35 and 15))

        assertEquals("east", stairs?.properties?.get("facing"))
    }

    @Test
    fun everyProceduralPieceGeometryMatchesVanilla26_2() {
        data class Vector(
            val type: NetherFortressPieces.PieceType,
            val width: Int,
            val height: Int,
            val depth: Int,
            val count: Int,
            val checksum: Long,
            val selfSeed: Int? = null,
        )

        // parity: 26.2 piece subclasses with generateBox/placeBlock recorded in local coordinates.
        val vectors =
            listOf(
                Vector(NetherFortressPieces.PieceType.BRIDGE_CROSSING, 19, 10, 19, 670, 6270398056101411L),
                Vector(NetherFortressPieces.PieceType.BRIDGE_END_FILLER, 5, 10, 8, 99, 7401052627000L, -1155484576),
                Vector(NetherFortressPieces.PieceType.BRIDGE_STRAIGHT, 5, 10, 19, 368, 1557764065606441L),
                Vector(NetherFortressPieces.PieceType.ROOM_CROSSING, 7, 9, 7, 182, 25631036366463L),
                Vector(NetherFortressPieces.PieceType.STAIRS_ROOM, 7, 11, 7, 289, 64397536388873L),
                Vector(NetherFortressPieces.PieceType.MONSTER_THRONE, 7, 8, 9, 230, 40637116622907L),
                Vector(NetherFortressPieces.PieceType.CASTLE_STAIRS, 5, 14, 10, 384, 448069602477840L),
                Vector(NetherFortressPieces.PieceType.CASTLE_T_BALCONY, 9, 7, 9, 314, 77907337616351L),
                Vector(NetherFortressPieces.PieceType.CASTLE_ENTRANCE, 13, 14, 13, 1479, 28720889878205969L),
                Vector(NetherFortressPieces.PieceType.CASTLE_SMALL_CORRIDOR, 5, 7, 5, 115, 10002555093980L),
                Vector(NetherFortressPieces.PieceType.CASTLE_SMALL_CROSSING, 5, 7, 5, 91, 6271172833880L),
                Vector(NetherFortressPieces.PieceType.CASTLE_LEFT_TURN, 5, 7, 5, 115, 10030621534176L),
                Vector(NetherFortressPieces.PieceType.CASTLE_RIGHT_TURN, 5, 7, 5, 115, 9950892956600L),
                Vector(NetherFortressPieces.PieceType.CASTLE_STALK_ROOM, 13, 14, 13, 1559, 29424107201548141L),
            )
        for (vector in vectors) {
            val piece =
                NetherFortressPieces.Piece(
                    vector.type,
                    0,
                    StructureBoundingBox(0, 20, 0, vector.width - 1, 20 + vector.height - 1, vector.depth - 1),
                    StructureDirection.NORTH,
                    selfSeed = vector.selfSeed,
                )
            val blocks = placedBlocks(piece, vector.width, vector.depth)

            assertEquals(vector.count, blocks.size, vector.type.name)
            assertEquals(
                vector.checksum,
                blocks.entries.withIndex().sumOf { (index, entry) ->
                    (index + 1L) * (31L * entry.key.hashCode() + entry.value)
                },
                vector.type.name,
            )
        }
    }

    @Test
    fun seedZeroLayoutMatchesVanilla26_2() {
        val random = WorldgenRandom(LegacyRandom(0L))
        random.setLargeFeatureSeed(0L, 0, 0)

        val actual = layout(random)

        val expected =
            """
            START|0|SOUTH|2,52,2,20,61,20
            STAIRS_ROOM|1|SOUTH|8,55,21,14,65,27
            BRIDGE_CROSSING|1|WEST|-17,52,2,1,61,20
            BRIDGE_STRAIGHT|1|EAST|21,52,9,39,61,13
            STAIRS_ROOM|2|EAST|40,55,8,46,65,14
            ROOM_CROSSING|2|EAST|15,61,21,21,69,27
            BRIDGE_CROSSING|3|EAST|22,58,15,40,67,33
            ROOM_CROSSING|3|SOUTH|15,61,28,21,69,34
            BRIDGE_CROSSING|2|WEST|-36,52,2,-18,61,20
            BRIDGE_STRAIGHT|2|NORTH|-10,52,-17,-6,61,1
            CASTLE_ENTRANCE|2|SOUTH|-14,52,21,-2,65,33
            ROOM_CROSSING|4|SOUTH|15,61,35,21,69,41
            MONSTER_THRONE|4|WEST|6,61,28,14,68,34
            CASTLE_SMALL_CROSSING|3|SOUTH|-10,55,34,-6,61,38
            STAIRS_ROOM|3|NORTH|-11,55,-24,-5,65,-18
            BRIDGE_CROSSING|5|SOUTH|9,58,42,27,67,60
            BRIDGE_STRAIGHT|5|WEST|-4,58,36,14,67,40
            BRIDGE_STRAIGHT|5|EAST|22,58,36,40,67,40
            BRIDGE_STRAIGHT|6|EAST|41,58,36,59,67,40
            BRIDGE_STRAIGHT|4|EAST|41,58,22,59,67,26
            BRIDGE_STRAIGHT|7|EAST|60,58,36,78,67,40
            CASTLE_RIGHT_TURN|4|SOUTH|-10,55,39,-6,61,43
            CASTLE_STAIRS|4|WEST|-20,48,34,-11,61,38
            BRIDGE_STRAIGHT|4|EAST|-4,58,-23,14,67,-19
            BRIDGE_STRAIGHT|6|SOUTH|16,58,61,20,67,79
            MONSTER_THRONE|6|WEST|0,61,48,8,68,54
            BRIDGE_STRAIGHT|6|EAST|28,58,49,46,67,53
            BRIDGE_STRAIGHT|7|SOUTH|16,58,80,20,67,98
            BRIDGE_STRAIGHT|8|EAST|79,58,36,97,67,40
            BRIDGE_STRAIGHT|3|WEST|-55,52,9,-37,61,13
            ROOM_CROSSING|3|NORTH|-30,55,-5,-24,63,1
            BRIDGE_END_FILLER|3|SOUTH|-29,52,21,-25,61,28
            BRIDGE_END_FILLER|5|EAST|15,58,-23,22,67,-19
            CASTLE_LEFT_TURN|5|WEST|-25,48,34,-21,54,38
            BRIDGE_END_FILLER|4|NORTH|-29,52,-13,-25,61,-6
            BRIDGE_END_FILLER|4|WEST|-38,52,-4,-31,61,0
            BRIDGE_END_FILLER|4|EAST|-23,52,-4,-16,61,0
            BRIDGE_END_FILLER|8|SOUTH|16,58,99,20,67,106
            BRIDGE_END_FILLER|9|EAST|98,58,36,105,67,40
            BRIDGE_END_FILLER|5|EAST|60,58,22,67,67,26
            BRIDGE_END_FILLER|7|EAST|47,58,49,54,67,53
            BRIDGE_END_FILLER|4|WEST|-63,52,9,-56,61,13
            CASTLE_RIGHT_TURN|6|NORTH|-25,48,29,-21,54,33
            CASTLE_SMALL_CORRIDOR|7|EAST|-20,48,29,-16,54,33
            """.trimIndent().lines()
        assertEquals(expected, actual)
    }

    @Test
    fun additionalLayoutsMatchVanilla26_2Digests() {
        // parity: official StartPiece assembly at chunk 0,0, serialized in piece insertion order.
        for ((seed, expectedCount, expectedChecksum) in listOf(
            Triple(1L, 48, -131679031755L),
            Triple(42L, 90, 626707768262L),
        )) {
            val random = WorldgenRandom(LegacyRandom(0L))
            random.setLargeFeatureSeed(seed, 0, 0)
            val layout = layout(random)
            assertEquals(expectedCount, layout.size)
            assertEquals(
                expectedChecksum,
                layout.withIndex().sumOf { (index, value) ->
                    (index + 1L) * value.hashCode()
                },
            )
        }
    }

    private fun layout(random: WorldgenRandom): List<String> =
        NetherFortressPieces.assemble(random, 0, 0).map { piece ->
            val box = piece.boundingBox
            "${piece.type}|${piece.generationDepth}|${piece.orientation}|" +
                "${box.minX},${box.minY},${box.minZ},${box.maxX},${box.maxY},${box.maxZ}"
        }

    private fun clip(chunk: Chunk): StructureBoundingBox =
        StructureBoundingBox(
            chunk.chunkX * 16,
            chunk.verticalBounds.minY,
            chunk.chunkZ * 16,
            chunk.chunkX * 16 + 15,
            chunk.verticalBounds.maxY,
            chunk.chunkZ * 16 + 15,
        )

    private fun placedBlocks(
        piece: NetherFortressPieces.Piece,
        width: Int,
        depth: Int,
    ): Map<String, Int> {
        val chunks =
            buildList {
                for (chunkX in 0..(width - 1).floorDiv(16)) {
                    for (chunkZ in 0..(depth - 1).floorDiv(16)) add(Chunk(chunkX, chunkZ, ChunkHeight.NETHER))
                }
            }
        val random = WorldgenRandom(LegacyRandom(0L))
        for (chunk in chunks) piece.place(chunk, clip(chunk), random)
        val spawner = Blocks.REGISTRY.byIdentifier(Identifier.vanilla("spawner"))?.stateId
        val blocks =
            buildMap {
                for (localX in 0 until width) {
                    for (localY in 0..14) {
                        for (localZ in 0 until depth) {
                            val worldZ = depth - 1 - localZ
                            val chunk =
                                chunks.first {
                                    it.chunkX ==
                                        localX.floorDiv(
                                            16,
                                        ) &&
                                        it.chunkZ == worldZ.floorDiv(16)
                                }
                            val stateId = chunk.getBlockStateId(localX and 15, 20 + localY, worldZ and 15)
                            if (!Blocks.isAir(stateId) && stateId != spawner) put("$localX,$localY,$localZ", stateId)
                        }
                    }
                }
            }
        return blocks.entries.sortedBy { it.key }.associate { it.key to it.value }
    }
}
