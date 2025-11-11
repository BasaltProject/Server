package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.structure.StructurePlacementGrid
import org.basaltmc.server.world.structure.StructureTemplate
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.Test

/** The frozen Kotlin as the oracle for the seven ruined portals. */
class RuinedPortalGeneratorOracle {
    private val portalNames =
        (1..10).map { "ruined_portal/portal_$it" } + (1..3).map { "ruined_portal/giant_portal_$it" }

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    /**
     * A stand-in portal frame: obsidian round a hollow, a structure void where the air pocket shows,
     * and a slab of stone brick the ager can rot. Nothing here comes from a Mojang structure file.
     */
    private fun frame(index: Int): StructureTemplate {
        val sizeX = 5 + index % 4
        val sizeY = 5 + index % 3
        val sizeZ = 3 + index % 2
        val palette =
            listOf(
                "minecraft:obsidian",
                "minecraft:stone_bricks",
                "minecraft:structure_void",
                "minecraft:air",
                "minecraft:lava",
                // Cobblestone is what the blackstone swap looks for and the ager leaves alone.
                "minecraft:cobblestone",
                // A slab is not a full block, so lava flows into its cell instead of it.
                "minecraft:stone_brick_slab",
            ).map { name -> NbtCompound().also { it.putString("Name", name) } }
        val blocks = mutableListOf<NbtCompound>()
        for (x in 0 until sizeX) {
            for (y in 0 until sizeY) {
                for (z in 0 until sizeZ) {
                    val entry = NbtCompound()
                    entry["pos"] = NbtList(NbtType.INT, listOf(NbtInt(x), NbtInt(y), NbtInt(z)))
                    val edge = x == 0 || x == sizeX - 1 || y == 0 || y == sizeY - 1
                    entry.putInt(
                        "state",
                        when {
                            edge && (x + y + z) % 3 == 0 -> 1
                            edge && (x + y + z) % 3 == 1 -> 5
                            edge && z == sizeZ - 1 -> 6
                            edge -> 0
                            z == 0 -> 2
                            y == 1 && x == 2 -> 4
                            else -> 3
                        },
                    )
                    blocks += entry
                }
            }
        }
        val nbt = NbtCompound()
        nbt["size"] = NbtList(NbtType.INT, listOf(NbtInt(sizeX), NbtInt(sizeY), NbtInt(sizeZ)))
        nbt["palette"] = NbtList(NbtType.COMPOUND, palette)
        nbt["blocks"] = NbtList(NbtType.COMPOUND, blocks)
        return StructureTemplate().also { it.load(nbt) }
    }

    private val templates: Map<String, StructureTemplate> =
        portalNames.withIndex().associate { (index, name) -> name to frame(index) }

    /** The seven variants, each with the placements and trimmings its real counterpart has. */
    private val variants =
        listOf(
            RuinedPortalVariant(
                "ruined_portal",
                setOf("minecraft:plains", "minecraft:forest"),
                listOf(
                    RuinedPortalSetup(
                        RuinedPortalPlacement.ON_LAND_SURFACE,
                        0.0f,
                        0.2f,
                        overgrown = false,
                        vines = false,
                        canBeCold = true,
                        replaceWithBlackstone = false,
                        weight = 0.8f,
                    ),
                    RuinedPortalSetup(
                        // Deliberately unlike the first: which of the two is drawn has to show.
                        RuinedPortalPlacement.PARTLY_BURIED,
                        0.0f,
                        0.9f,
                        overgrown = true,
                        vines = true,
                        canBeCold = false,
                        replaceWithBlackstone = true,
                        weight = 0.2f,
                    ),
                ),
            ),
            RuinedPortalVariant(
                "ruined_portal_desert",
                setOf("minecraft:desert"),
                listOf(
                    RuinedPortalSetup(
                        RuinedPortalPlacement.PARTLY_BURIED,
                        0.0f,
                        0.0f,
                        overgrown = false,
                        vines = false,
                        canBeCold = false,
                        replaceWithBlackstone = false,
                        weight = 1.0f,
                    ),
                ),
            ),
            RuinedPortalVariant(
                "ruined_portal_jungle",
                setOf("minecraft:jungle"),
                listOf(
                    RuinedPortalSetup(
                        RuinedPortalPlacement.ON_LAND_SURFACE,
                        0.5f,
                        0.8f,
                        overgrown = true,
                        vines = true,
                        canBeCold = false,
                        replaceWithBlackstone = false,
                        weight = 1.0f,
                    ),
                ),
            ),
            RuinedPortalVariant(
                "ruined_portal_mountain",
                setOf("minecraft:snowy_slopes", "minecraft:grove"),
                listOf(
                    RuinedPortalSetup(
                        RuinedPortalPlacement.IN_MOUNTAIN,
                        0.5f,
                        0.2f,
                        overgrown = false,
                        vines = false,
                        canBeCold = true,
                        replaceWithBlackstone = false,
                        weight = 1.0f,
                    ),
                ),
            ),
            RuinedPortalVariant(
                "ruined_portal_nether",
                setOf("minecraft:nether_wastes"),
                listOf(
                    RuinedPortalSetup(
                        RuinedPortalPlacement.IN_NETHER,
                        0.5f,
                        0.0f,
                        overgrown = false,
                        vines = false,
                        canBeCold = false,
                        replaceWithBlackstone = true,
                        weight = 1.0f,
                    ),
                ),
            ),
            RuinedPortalVariant(
                "ruined_portal_ocean",
                setOf("minecraft:ocean"),
                listOf(
                    RuinedPortalSetup(
                        RuinedPortalPlacement.ON_OCEAN_FLOOR,
                        0.0f,
                        0.8f,
                        overgrown = false,
                        vines = false,
                        canBeCold = false,
                        replaceWithBlackstone = false,
                        weight = 1.0f,
                    ),
                ),
            ),
            RuinedPortalVariant(
                "ruined_portal_swamp",
                setOf("minecraft:swamp"),
                listOf(
                    RuinedPortalSetup(
                        RuinedPortalPlacement.UNDERGROUND,
                        0.5f,
                        0.5f,
                        overgrown = false,
                        vines = true,
                        canBeCold = false,
                        replaceWithBlackstone = false,
                        weight = 1.0f,
                    ),
                ),
            ),
        )

    /** The generator keeps its grid to itself, so the sweep rebuilds the same one to centre on. */
    private val grid = StructurePlacementGrid(spacing = 40, separation = 15, salt = 34222645)

    private fun surfaceAt(
        x: Int,
        z: Int,
    ): Int = 70 + (x * 2 + z * 3).mod(6)

    private fun oceanFloorAt(
        x: Int,
        z: Int,
    ): Int = 52 + (x + z * 2).mod(4)

    /**
     * Lava lakes that reach the surface, so a frame can land inside one: that is the only shape in
     * which the submerged processor and the lava branch of the apron have anything to act on.
     */
    private fun lavaColumn(
        x: Int,
        z: Int,
    ): Boolean = x.mod(9) < 5 && z.mod(11) < 5

    private fun terrainAt(
        x: Int,
        y: Int,
        z: Int,
    ): Int =
        when {
            lavaColumn(x, z) && y < surfaceAt(x, z) -> state("lava")
            y < oceanFloorAt(x, z) -> Blocks.STONE.stateId
            y < surfaceAt(x, z) -> state("water")
            else -> Blocks.AIR.stateId
        }

    private fun portalChunk(
        chunkX: Int,
        chunkZ: Int,
    ): Chunk {
        val chunk = Chunk(chunkX, chunkZ)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 20..100) {
                    chunk.setBlockStateId(x, y, z, terrainAt(chunkX * 16 + x, y, chunkZ * 16 + z))
                }
            }
        }
        return chunk
    }

    private fun fold(chunk: Chunk): Triple<Long, Int, Int> {
        var checksum = -3750763034362895579L
        var written = 0
        for (y in 20..110) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    checksum = (checksum xor id.toLong()) * 1099511628211L
                    if (id != terrainAt(chunk.chunkX * 16 + x, y, chunk.chunkZ * 16 + z)) written++
                }
            }
        }
        val deferred = chunk.takeGeneratedBlockWrites()
        for (write in deferred) {
            checksum = (checksum xor write.x.toLong() xor (write.y.toLong() shl 20) xor (write.z.toLong() shl 40)) *
                1099511628211L
            checksum = (checksum xor write.stateId.toLong()) * 1099511628211L
        }
        return Triple(checksum, written, deferred.size)
    }

    private fun report(
        name: String,
        seed: Long,
        biome: String,
    ) {
        val generator =
            RuinedPortalStructureGenerator(
                variants = variants,
                templateOf = { templates[it] },
                // Only the column the portal starts on answers, so where the biome is read matters.
                biomeAt = { x, _, z -> if (x.mod(16) == 0 && z.mod(16) == 0) biome else "minecraft:the_void" },
                surfaceHeightAt = { x, z -> surfaceAt(x, z) },
                oceanFloorAt = { x, z -> oceanFloorAt(x, z) },
                isSolidAt = { x, y, z -> y < oceanFloorAt(x, z) },
            )
        val centre = grid.candidateChunk(seed, 0, 0)
        var checksum = -3750763034362895579L
        var written = 0
        var deferred = 0
        for (index in 0 until 64) {
            val chunk = portalChunk(centre.first + index % 8 - 4, centre.second + index / 8 - 4)
            generator.generateInto(chunk, seed)
            val (chunkChecksum, chunkWritten, chunkDeferred) = fold(chunk)
            checksum = (checksum xor chunkChecksum) * 1099511628211L
            written += chunkWritten
            deferred += chunkDeferred
        }
        println("ORACLE $name written=$written deferred=$deferred checksum=$checksum")
    }

    @Test
    fun dump() {
        report("plains", 1234L, "minecraft:plains")
        report("desert", 1234L, "minecraft:desert")
        report("jungle", 1234L, "minecraft:jungle")
        report("mountain", 1234L, "minecraft:snowy_slopes")
        report("nether", 1234L, "minecraft:nether_wastes")
        report("ocean", 1234L, "minecraft:ocean")
        report("swamp", 1234L, "minecraft:swamp")
        report("none", 1234L, "minecraft:the_void")
        report("otherSeed", 77L, "minecraft:jungle")
        // More plains, which is the one variant with two setups to draw between.
        report("plainsOther", 77L, "minecraft:plains")
        report("plainsThird", 9001L, "minecraft:plains")
        report("plainsFourth", 4321L, "minecraft:plains")
        report("plainsFifth", 5L, "minecraft:plains")
        report("plainsSixth", 13L, "minecraft:plains")
        report("plainsSeventh", 21L, "minecraft:plains")
        report("plainsEighth", 34L, "minecraft:plains")
    }
}
