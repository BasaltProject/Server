package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component
import kotlin.math.roundToInt
import kotlin.math.sqrt

/**
 * parity: `LocateCommand`'s `biome` form, searched the way vanilla searches: a widening square
 * scan at quart resolution around the runner, first hit wins.
 *
 * `structure` and `poi` are not declared. A structure search asks the generator "where would the
 * next one be", which needs the placement grid exposed per structure set rather than the
 * chunk-scoped generation this server does, and points of interest do not exist at all yet.
 */
public class LocateCommand : Command(name = "locate", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("biome") {
            argument("biome", ResourceLocationArgument) { executes { context -> locateBiome(context) } }
        }
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
    }
}
