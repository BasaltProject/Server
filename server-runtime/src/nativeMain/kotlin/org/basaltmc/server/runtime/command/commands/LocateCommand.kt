package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.PointsOfInterest
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.world.terrain.VillageStructureGenerator
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component
import kotlin.math.roundToInt
import kotlin.math.sqrt

/**
 * parity: `LocateCommand`, all three forms. `biome` is searched the way vanilla searches, a widening
 * square scan at quart resolution around the runner; `structure` walks the structure set's placement
 * grid outward and tests each candidate exactly as generation would; `poi` looks for the blocks the
 * type is made of.
 *
 * The difference in `poi` is where it looks. Vanilla keeps a point-of-interest index per chunk,
 * written as chunks generate, and asks that; this build has no such index, so it reads the chunks
 * themselves, which is why it only finds what is loaded. What makes that affordable is the palette:
 * a section that does not hold the block at all is skipped without reading a single one.
 */
public class LocateCommand : Command(name = "locate", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("biome") {
            argument("biome", RegistryArgument.orTag("minecraft:worldgen/biome")) {
                suggestsFromServer { context -> context.matchingIds(Biomes.REGISTRY.identifiers()) }
                executes { context -> locateBiome(context) }
            }
        }
        root.literal("structure") {
            argument("structure", RegistryArgument.orTagKey("minecraft:worldgen/structure")) {
                suggestsFromServer { context -> context.matchingIds(VillageStructureGenerator.STRUCTURE_IDS) }
                executes { context -> locateStructure(context) }
            }
        }
        root.literal("poi") {
            argument("poi", RegistryArgument.orTag("minecraft:point_of_interest_type")) {
                suggestsFromServer { context -> context.matchingIds(PointsOfInterest.ids) }
                executes { context -> locatePoi(context) }
            }
        }
    }

    /** parity: `LocateCommand.locatePoi`, which searches 256 blocks around the runner and takes the nearest. */
    private fun locatePoi(context: CommandContext): Int {
        val world = context.world()
        val wanted = context.get<Identifier>("poi")
        val blocks =
            PointsOfInterest[wanted]
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.locate.poi.not_found", Component.text(wanted.toString())),
                )
        val (originX, originY, originZ) = context.source.position
        val startX = originX.roundToInt()
        val startY = originY.roundToInt()
        val startZ = originZ.roundToInt()

        var bestX = 0
        var bestY = 0
        var bestZ = 0
        var bestDistance = Long.MAX_VALUE
        for (chunkX in ((startX - POI_RADIUS) shr 4)..((startX + POI_RADIUS) shr 4)) {
            for (chunkZ in ((startZ - POI_RADIUS) shr 4)..((startZ + POI_RADIUS) shr 4)) {
                val chunk = world.state.chunkIfLoaded(chunkX, chunkZ) ?: continue
                for (index in 0 until chunk.sectionCount) {
                    val section = chunk.section(index)
                    if (!section.mayContain { Blocks.byStateId(it)?.block in blocks }) continue
                    val sectionMinY = chunk.verticalBounds.minY + index * SECTION_HEIGHT
                    for (localY in 0 until SECTION_HEIGHT) {
                        for (localZ in 0 until 16) {
                            for (localX in 0 until 16) {
                                val state = section.getBlockStateId(localX, localY, localZ)
                                if (Blocks.byStateId(state)?.block !in blocks) continue
                                val x = chunkX * 16 + localX
                                val y = sectionMinY + localY
                                val z = chunkZ * 16 + localZ
                                val dx = (x - startX).toLong()
                                val dy = (y - startY).toLong()
                                val dz = (z - startZ).toLong()
                                val distance = dx * dx + dy * dy + dz * dz
                                if (distance >= bestDistance) continue
                                bestDistance = distance
                                bestX = x
                                bestY = y
                                bestZ = z
                            }
                        }
                    }
                }
            }
        }
        if (bestDistance == Long.MAX_VALUE) {
            throw CommandSyntaxException(
                Component.translatable("commands.locate.poi.not_found", Component.text(wanted.toString())),
            )
        }
        val distance = sqrt(bestDistance.toDouble()).roundToInt()
        context.success(
            Component.translatable(
                "commands.locate.poi.success",
                Component.text(wanted.toString()),
                Component.text("$bestX, $bestY, $bestZ"),
                Component.text(distance.toString()),
            ),
        )
        return distance
    }

    /** parity: `LocateCommand.locateStructure`, which reports the block position and the distance in blocks. */
    private fun locateStructure(context: CommandContext): Int {
        val world = context.world()
        val wanted = context.get<Identifier>("structure")
        val villages =
            world.villageGenerator
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.locate.structure.not_found", Component.text(wanted.toString())),
                )
        if (wanted !in villages.structureIds) {
            throw CommandSyntaxException(
                Component.text(
                    "'$wanted' is not a structure this build generates. Available: " +
                        villages.structureIds.joinToString(", "),
                ),
            )
        }
        val (originX, _, originZ) = context.source.position
        val startX = originX.roundToInt()
        val startZ = originZ.roundToInt()
        val found =
            villages.locate(world.metadata.seed, startX shr 4, startZ shr 4, wanted)
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.locate.structure.not_found", Component.text(wanted.toString())),
                )
        val x = found.chunkX * 16 + 8
        val z = found.chunkZ * 16 + 8
        val distance =
            sqrt(((x - startX).toDouble() * (x - startX) + (z - startZ).toDouble() * (z - startZ))).roundToInt()
        context.success(
            Component.translatable(
                "commands.locate.structure.success",
                Component.text(wanted.toString()),
                Component.text("$x, ~, $z"),
                Component.text(distance.toString()),
            ),
        )
        return distance
    }

    private fun locateBiome(context: CommandContext): Int {
        val world = context.world()
        val source =
            world.biomeSource
                ?: throw CommandSyntaxException(
                    Component.text(
                        "World '${world.name}' has no biome source to search: only a normal Overworld does.",
                    ),
                )
        val wanted = context.get<Identifier>("biome")
        val (originX, originY, originZ) = context.source.position
        val startX = originX.roundToInt()
        val startY = originY.roundToInt()
        val startZ = originZ.roundToInt()

        // parity: `BiomeSource.findClosestBiome3d`, which steps in quarts and gives up at 6400.
        var radius = 0
        while (radius <= MAX_RADIUS_BLOCKS) {
            for (dx in -radius..radius step STEP_BLOCKS) {
                for (dz in -radius..radius step STEP_BLOCKS) {
                    // Only the ring just added, so a hit is the closest one found so far.
                    if (radius > 0 && dx * dx != radius * radius && dz * dz != radius * radius) continue
                    val x = startX + dx
                    val z = startZ + dz
                    if (source.biomeAt(x, startY, z).id != wanted) continue
                    val distance = sqrt(((x - startX) * (x - startX) + (z - startZ) * (z - startZ)).toDouble())
                    context.success(
                        Component.translatable(
                            "commands.locate.biome.success",
                            Component.text(wanted.toString()),
                            Component.text("$x, ~, $z"),
                            Component.text(distance.roundToInt().toString()),
                        ),
                    )
                    return distance.roundToInt()
                }
            }
            radius += STEP_BLOCKS
        }
        throw CommandSyntaxException(
            Component.translatable("commands.locate.biome.not_found", Component.text(wanted.toString())),
        )
    }

    private companion object {
        /** parity: vanilla's own search limits for a biome: every 32 blocks, out to 6400. */
        const val STEP_BLOCKS = 32
        const val MAX_RADIUS_BLOCKS = 6400

        /** parity: `PoiManager.findClosestWithType`'s own radius, in blocks. */
        const val POI_RADIUS = 256
        const val SECTION_HEIGHT = 16
    }
}
