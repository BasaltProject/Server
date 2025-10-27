package org.basaltmc.server.world.structure

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the jigsaw assembly. */
class JigsawAssemblyOracle {
    private data class Connector(
        val x: Int,
        val y: Int,
        val z: Int,
        val orientation: String,
        val name: String,
        val pool: String,
        val target: String,
        val rollable: Boolean = true,
        val placementPriority: Int = 0,
        val selectionPriority: Int = 0,
    )

    private val orientations =
        listOf("north_up", "south_up", "west_up", "east_up", "up_north", "down_north", "down_east")

    /**
     * A synthetic piece: a box of stone with one jigsaw block per connector. Nothing here comes from
     * a Mojang structure file; what is under test is the assembly, not the buildings.
     */
    private fun piece(
        sizeX: Int,
        sizeY: Int,
        sizeZ: Int,
        connectors: List<Connector>,
    ): StructureTemplate {
        val entries = mutableListOf(NbtCompound().also { it.putString("Name", "minecraft:stone") })
        for (orientation in orientations) {
            val properties = NbtCompound()
            properties["orientation"] = NbtString(orientation)
            entries +=
                NbtCompound().also {
                    it.putString("Name", "minecraft:jigsaw")
                    it["Properties"] = properties
                }
        }
        val palette = NbtList(NbtType.COMPOUND, entries)

        val blocks = mutableListOf<NbtCompound>()
        val taken = connectors.map { Triple(it.x, it.y, it.z) }.toSet()
        for (x in 0 until sizeX) {
            for (y in 0 until sizeY) {
                for (z in 0 until sizeZ) {
                    if (Triple(x, y, z) in taken) continue
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    entry.putInt("state", 0)
                    blocks += entry
                }
            }
        }
        for (connector in connectors) {
            val entry = NbtCompound()
            entry["pos"] =
                NbtList(NbtType.INT, listOf(NbtInt(connector.x), NbtInt(connector.y), NbtInt(connector.z)))
            entry.putInt("state", 1 + orientations.indexOf(connector.orientation))
            val data = NbtCompound()
            data["name"] = NbtString(connector.name)
            data["pool"] = NbtString(connector.pool)
            data["target"] = NbtString(connector.target)
            data["final_state"] = NbtString("minecraft:air")
            data["joint"] = NbtString(if (connector.rollable) "rollable" else "aligned")
            data["placement_priority"] = NbtInt(connector.placementPriority)
            data["selection_priority"] = NbtInt(connector.selectionPriority)
            entry["nbt"] = data
            blocks += entry
        }
        val nbt = NbtCompound()
        nbt["size"] = NbtList(NbtType.INT, listOf(NbtInt(sizeX), NbtInt(sizeY), NbtInt(sizeZ)))
        nbt["palette"] = palette
        nbt["blocks"] = NbtList(NbtType.COMPOUND, blocks)
        return StructureTemplate().also { it.load(nbt) }
    }

    private val templates: Map<String, StructureTemplate> =
        mapOf(
            "minecraft:test/start" to
                piece(
                    9,
                    7,
                    9,
                    listOf(
                        // Two connectors of equal priority, so the queue's insertion-order tie-break
                        // is the only thing deciding which one is expanded first.
                        Connector(8, 1, 4, "east_up", "start_a", "minecraft:test/wings", "wing_in"),
                        Connector(0, 1, 4, "west_up", "start_b", "minecraft:test/wings", "wing_in"),
                        // Aligned and vertical: the joint has to match tops, and the front carries a
                        // dy that the offset between the two pieces depends on.
                        Connector(
                            4,
                            6,
                            4,
                            "up_north",
                            "start_c",
                            "minecraft:test/towers",
                            "tower_in",
                            rollable = false,
                            placementPriority = 1,
                        ),
                        // Points into its own box, so its children use the piece's own free space
                        // rather than the shared one.
                        // Small enough to fit inside the start piece, which is the point: in the
                        // shared space it would collide with its own parent.
                        Connector(4, 1, 4, "east_up", "start_d", "minecraft:test/pebbles", "pebble_in"),
                    ),
                ),
            "minecraft:test/wing_big" to
                piece(
                    5,
                    5,
                    5,
                    listOf(
                        Connector(0, 1, 2, "west_up", "wing_in", "minecraft:test/leaves", "leaf_in"),
                        Connector(4, 1, 2, "east_up", "wing_out", "minecraft:test/leaves", "leaf_in"),
                        // Points back into its own box, which is the only shape the expansion hack
                        // grows a piece for.
                        Connector(2, 1, 2, "east_up", "wing_inner", "minecraft:test/leaves", "leaf_in"),
                    ),
                ),
            "minecraft:test/wing_small" to
                piece(3, 5, 3, listOf(Connector(0, 1, 1, "west_up", "wing_in", "minecraft:empty", "empty"))),
            // Only reachable through the wings pool's fallback.
            "minecraft:test/wing_spare" to
                piece(3, 3, 3, listOf(Connector(0, 1, 1, "west_up", "wing_in", "minecraft:empty", "empty"))),
            "minecraft:test/tower_north" to
                piece(3, 9, 3, listOf(Connector(1, 0, 1, "down_north", "tower_in", "minecraft:empty", "empty"))),
            // The same shape with the wrong top, which an aligned joint refuses.
            "minecraft:test/tower_east" to
                piece(3, 5, 3, listOf(Connector(1, 0, 1, "down_east", "tower_in", "minecraft:empty", "empty"))),
            // Tall on purpose: the expansion hack grows a piece to fit its children, and a child no
            // taller than its parent leaves nothing to grow.
            "minecraft:test/leaf" to
                piece(3, 9, 3, listOf(Connector(0, 1, 1, "west_up", "leaf_in", "minecraft:empty", "empty"))),
            "minecraft:test/pebble" to
                piece(2, 2, 2, listOf(Connector(0, 0, 0, "west_up", "pebble_in", "minecraft:empty", "empty"))),
        )

    private val loader: TemplateLoader = { name -> templates[name] }

    private fun single(
        location: String,
        projection: StructureProjection = StructureProjection.RIGID,
    ): StructurePoolElement = LegacySinglePoolElement(location, StructureProcessorList.EMPTY, projection)

    private val pools: Map<String, StructureTemplatePool> =
        mapOf(
            "minecraft:empty" to StructureTemplatePool("minecraft:empty", listOf(EmptyPoolElement to 1)),
            "minecraft:test/start" to
                StructureTemplatePool("minecraft:empty", listOf(single("minecraft:test/start") to 1)),
            "minecraft:test/wings" to
                StructureTemplatePool(
                    "minecraft:test/wings_spare",
                    listOf(single("minecraft:test/wing_big") to 3, single("minecraft:test/wing_small") to 1),
                ),
            "minecraft:test/wings_spare" to
                StructureTemplatePool("minecraft:empty", listOf(single("minecraft:test/wing_spare") to 1)),
            "minecraft:test/towers" to
                StructureTemplatePool(
                    "minecraft:empty",
                    listOf(single("minecraft:test/tower_east") to 3, single("minecraft:test/tower_north") to 1),
                ),
            "minecraft:test/leaves" to
                StructureTemplatePool("minecraft:empty", listOf(single("minecraft:test/leaf") to 1)),
            "minecraft:test/pebbles" to
                StructureTemplatePool("minecraft:empty", listOf(single("minecraft:test/pebble") to 1)),
        )

    private fun describe(piece: PlacedStructurePiece): String {
        val box = piece.boundingBox
        val location = (piece.element as? LegacySinglePoolElement)?.location ?: "empty"
        return "$location@${piece.position.x},${piece.position.y},${piece.position.z}/${piece.rotation.name}/" +
            "${box.minX},${box.minY},${box.minZ},${box.maxX},${box.maxY},${box.maxZ}/" +
            "${piece.groundLevelDelta}/${piece.junctions.size}"
    }

    private fun report(
        name: String,
        seed: Long,
        maxDepth: Int,
        projectStartToHeightmap: Boolean,
        maxDistanceFromCenter: Int,
        useExpansionHack: Boolean,
        startJigsawName: String? = null,
    ) {
        val random = WorldgenRandom(XoroshiroRandom(seed))
        val assembly = JigsawAssembly({ pools[it] }, loader, { _, _ -> 64 })
        val pieces =
            assembly.assemble(
                "minecraft:test/start",
                StructurePos(0, 0, 0),
                maxDepth,
                random,
                projectStartToHeightmap,
                maxDistanceFromCenter,
                useExpansionHack = useExpansionHack,
                startJigsawName = startJigsawName,
            )
        var checksum = -3750763034362895579L
        for (piece in pieces) {
            for (char in describe(piece)) checksum = (checksum xor char.code.toLong()) * 1099511628211L
            for (junction in piece.junctions) {
                for (value in listOf(
                    junction.sourceX,
                    junction.sourceGroundY,
                    junction.sourceZ,
                    junction.deltaY,
                    junction.destinationProjection.ordinal,
                )) {
                    checksum = (checksum xor value.toLong()) * 1099511628211L
                }
            }
        }
        println(
            "ORACLE $name pieces=${pieces.size} first=${pieces.firstOrNull()?.let(::describe)} " +
                "checksum=$checksum after=${random.nextInt()}",
        )
    }

    @Test
    fun dump() {
        report("depth0", 4321L, 0, false, 80, false)
        report("depth1", 4321L, 1, false, 80, false)
        report("depth3", 4321L, 3, false, 80, false)
        report("projected", 4321L, 3, true, 80, false)
        // The expansion hack grows a short piece so its own children still fit above it.
        report("expanded", 4321L, 3, false, 80, true)
        // A tight allowed region rejects most children before they are even tested for collision.
        report("tight", 4321L, 3, false, 6, false)
        // Naming a starting jigsaw shifts the whole assembly and costs a draw of its own.
        report("anchored", 4321L, 2, false, 80, false, startJigsawName = "start_b")
        report("otherSeed", 99L, 3, false, 80, false)
    }
}
