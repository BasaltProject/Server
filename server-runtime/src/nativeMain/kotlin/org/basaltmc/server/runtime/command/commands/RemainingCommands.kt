package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.RealDialogs
import org.basaltmc.server.runtime.world.RealEnchantments
import org.basaltmc.server.runtime.world.RealTestInstances
import org.basaltmc.server.runtime.world.StructureTemplates
import org.basaltmc.server.runtime.world.TestInstance
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.terrain.decoration.PlacedFeatures
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.ClearDialogPacket
import org.basaltmc.wire.protocol.play.ShowDialogPacket
import org.basaltmc.wire.protocol.play.TeamColor
import org.basaltmc.wire.protocol.play.TrackedWaypointPacket
import org.basaltmc.wire.text.Component
import kotlin.random.Random
import kotlin.uuid.ExperimentalUuidApi

/**
 * The locator-bar icon of an entity. parity: `WaypointCommand`.
 *
 * Every change is pushed with `tracked_waypoint`, so the icon really appears on the bar: the
 * waypoint is the positioned kind, identified by the entity's own UUID, which is how vanilla tracks
 * a player's.
 */
public class WaypointCommand : Command(name = "waypoint", permissionLevel = PermissionLevel.GAMEMASTERS) {
    /** Style and colour per entity id, which is what `list` reports and `modify` changes. */
    private val icons = mutableMapOf<Int, Icon>()

    private class Icon(
        var style: Identifier = Identifier.vanilla("default"),
        var color: Int? = null,
    )

    override fun build(root: CommandNodeBuilder) {
        root.literal("list") { executes { context -> list(context) } }
        root.literal("modify") {
            argument("target", EntitiesArgument(single = true)) {
                literal("color") {
                    literal("reset") { executes { context -> resetColor(context) } }
                    literal("hex") {
                        argument("color", WordArgument) { executes { context -> setHexColor(context) } }
                    }
                    for (color in TeamColor.entries) {
                        literal(color.name.lowercase()) { executes { context -> setNamedColor(context, color) } }
                    }
                }
                literal("style") {
                    literal("reset") { executes { context -> resetStyle(context) } }
                    literal("set") {
                        argument("style", ResourceLocationArgument) { executes { context -> setStyle(context) } }
                    }
                }
            }
        }
    }

    private fun iconOf(context: CommandContext): Icon {
        val target = context.get<List<CommandTarget>>("target").single()
        return icons.getOrPut(target.entityId) { Icon() }
    }

    /** Sends the target's waypoint to everyone in its world, which is what draws the bar entry. */
    @OptIn(ExperimentalUuidApi::class)
    private fun broadcast(context: CommandContext) {
        val target = context.get<List<CommandTarget>>("target").single()
        val icon = icons[target.entityId] ?: return
        val uuid =
            when (target) {
                is CommandTarget.Player -> target.session.uuid
                is CommandTarget.Entity -> target.entity.uuid
            }
        val packet =
            TrackedWaypointPacket(
                operation = TrackedWaypointPacket.Operation.TRACK,
                entityUuid = uuid,
                style = icon.style.toString(),
                color = icon.color,
                x = kotlin.math.floor(target.x).toInt(),
                y = kotlin.math.floor(target.y).toInt(),
                z = kotlin.math.floor(target.z).toInt(),
            )
        for (player in context.environment.players.snapshot()) {
            player.connection.sendPlayPacket(TrackedWaypointPacket.ID) { packet.write(it) }
        }
    }

    private fun list(context: CommandContext): Int {
        val tracked = icons.size
        context.success(
            if (tracked == 0) {
                Component.translatable("commands.waypoint.list.empty", Component.text(context.world().declaration.name))
            } else {
                Component.translatable(
                    "commands.waypoint.list.success",
                    Component.text(tracked.toString()),
                    Component.text(context.world().declaration.name),
                    Component.text(icons.keys.joinToString(", ")),
                )
            },
        )
        return tracked
    }

    private fun setNamedColor(
        context: CommandContext,
        color: TeamColor,
    ): Int {
        iconOf(context).color = color.rgb
        broadcast(context)
        context.success(
            Component.translatable("commands.waypoint.modify.color", Component.text(color.name.lowercase())),
        )
        return 0
    }

    private fun setHexColor(context: CommandContext): Int {
        val text = context.get<String>("color").removePrefix("#")
        val value =
            text.toIntOrNull(16)
                ?: throw CommandSyntaxException(Component.text("'$text' is not a hexadecimal colour."))
        iconOf(context).color = value
        broadcast(context)
        context.success(Component.translatable("commands.waypoint.modify.color", Component.text(text)))
        return 0
    }

    private fun resetColor(context: CommandContext): Int {
        iconOf(context).color = null
        broadcast(context)
        context.success(Component.translatable("commands.waypoint.modify.color.reset"))
        return 0
    }

    private fun setStyle(context: CommandContext): Int {
        iconOf(context).style = context.get("style")
        broadcast(context)
        context.success(Component.translatable("commands.waypoint.modify.style"))
        return 0
    }

    private fun resetStyle(context: CommandContext): Int {
        iconOf(context).style = Identifier.vanilla("default")
        broadcast(context)
        context.success(Component.translatable("commands.waypoint.modify.style"))
        return 0
    }
}

/**
 * parity: `PlaceCommand`. `template` works end to end, rotation and mirror included.
 *
 * `feature` covers what [PlacedFeatures] exposes by id; the rest, and `jigsaw` and `structure`,
 * are refused by name until generation can be asked for one thing at one position.
 */
public class PlaceCommand(
    private val templates: StructureTemplates,
) : Command(name = "place", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("feature") {
            argument("id", ResourceLocationArgument) {
                executes { context -> placeFeature(context, null) }
                argument("pos", BlockPosArgument) {
                    executes { context -> placeFeature(context, context.get("pos")) }
                }
            }
        }
        for (kind in listOf("jigsaw", "structure")) {
            root.literal(kind) {
                argument("id", ResourceLocationArgument) {
                    executes { context -> refuse(context, kind, context.get("id")) }
                }
            }
        }
        root.literal("template") {
            argument("id", ResourceLocationArgument) {
                executes { context -> place(context, null, StructureRotation.NONE, StructureMirror.NONE) }
                argument("pos", BlockPosArgument) {
                    executes { context ->
                        place(context, context.get("pos"), StructureRotation.NONE, StructureMirror.NONE)
                    }
                    for (rotation in StructureRotation.entries) {
                        literal(rotation.serializedName) {
                            executes { context ->
                                place(context, context.get("pos"), rotation, StructureMirror.NONE)
                            }
                            for (mirror in StructureMirror.entries) {
                                literal(mirror.serializedName) {
                                    executes { context -> place(context, context.get("pos"), rotation, mirror) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun place(
        context: CommandContext,
        coordinates: BlockCoordinates?,
        rotation: StructureRotation,
        mirror: StructureMirror,
    ): Int {
        val id = context.get<Identifier>("id")
        val template =
            templates[id]
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.place.template.invalid", Component.text(id.toString())),
                )
        val world = context.world()
        val (x, y, z) =
            coordinates?.resolve(context.source.position) ?: context.source.position.let { (px, py, pz) ->
                Triple(kotlin.math.floor(px).toInt(), kotlin.math.floor(py).toInt(), kotlin.math.floor(pz).toInt())
            }

        val origin = StructurePos(x, y, z)
        val pivot = StructurePos(0, 0, 0)
        val box = template.getBoundingBox(origin, rotation, pivot, mirror)
        var chunks = 0
        // One pass per intersecting chunk, which is what generation does: each pass keeps only the
        // blocks that land inside that chunk.
        for (chunkX in (box.minX shr 4)..(box.maxX shr 4)) {
            for (chunkZ in (box.minZ shr 4)..(box.maxZ shr 4)) {
                val chunk =
                    world.state.chunkIfLoaded(chunkX, chunkZ)
                        ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
                template.placeInto(
                    chunk = chunk,
                    position = origin,
                    rotation = rotation,
                    mirror = mirror,
                    pivot = pivot,
                    clipTo =
                        StructureBoundingBox(
                            chunkX shl 4,
                            box.minY,
                            chunkZ shl 4,
                            (chunkX shl 4) + 15,
                            box.maxY,
                            (chunkZ shl 4) + 15,
                        ),
                    ignoreStructureBlocks = true,
                )
                chunks++
            }
        }
        context.environment.syncChunks(world, box.minX, box.minZ, box.maxX, box.maxZ)
        context.success(
            Component.translatable(
                "commands.place.template.success",
                Component.text(id.toString()),
                Component.text(x.toString()),
                Component.text(y.toString()),
                Component.text(z.toString()),
            ),
            broadcastToAdmins = true,
        )
        return chunks
    }

    /** parity: `PlaceCommand.placeFeature`, which runs the feature once from the world's own random. */
    private fun placeFeature(
        context: CommandContext,
        coordinates: BlockCoordinates?,
    ): Int {
        val id = context.get<Identifier>("id")
        val placement =
            PlacedFeatures[id]
                ?: throw CommandSyntaxException(
                    Component.text(
                        "'$id' is not a feature that can be placed on its own yet. Generation runs it per chunk, " +
                            "not by id. Available: ${PlacedFeatures.ids.joinToString(", ")}.",
                    ),
                )
        val world = context.world()
        val (x, y, z) =
            coordinates?.resolve(context.source.position) ?: context.source.position.let { (px, py, pz) ->
                Triple(kotlin.math.floor(px).toInt(), kotlin.math.floor(py).toInt(), kotlin.math.floor(pz).toInt())
            }
        val chunk =
            world.state.chunkIfLoaded(x shr 4, z shr 4)
                ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))

        placement.place(chunk, WorldgenRandom(XoroshiroRandom(Random.nextLong())), x and 15, y, z and 15)
        context.environment.syncChunks(world, x, z, x, z)
        context.success(
            Component.translatable(
                "commands.place.feature.success",
                Component.text(id.toString()),
                Component.text(x.toString()),
                Component.text(y.toString()),
                Component.text(z.toString()),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun refuse(
        context: CommandContext,
        kind: String,
        id: Identifier,
    ): Int =
        throw CommandSyntaxException(
            Component.text(
                "Placing a $kind ('$id') on demand needs world generation to expose whole structures by id; " +
                    "they generate correctly, but nothing can look one up yet. `/place template` does work and " +
                    "places a structure file.",
            ),
        )
}

/**
 * parity: `EnchantCommand`.
 *
 * The enchantment goes on the held stack as the `minecraft:enchantments` component, which is what
 * the client draws and what `players.db` keeps. It needs the enchantment registry to have been sent
 * (see `RealEnchantments`), so on a server with no `server.jar` downloaded it refuses by name
 * rather than writing a component nothing can resolve.
 */
public class EnchantCommand(
    private val enchantments: RealEnchantments,
) : Command(name = "enchant", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", PlayersArgument(single = false)) {
            argument("enchantment", ResourceLocationArgument) {
                executes { context -> enchant(context, 1) }
                argument("level", IntegerArgument(min = 0, max = MAX_LEVEL)) {
                    executes { context -> enchant(context, context.get("level")) }
                }
            }
        }
    }

    private fun enchant(
        context: CommandContext,
        level: Int,
    ): Int {
        val id = context.get<Identifier>("enchantment")
        if (enchantments.size == 0) {
            throw CommandSyntaxException(
                Component.text(
                    "No enchantment registry has been sent to clients, so '$id' would not resolve on one. " +
                        "It is read from the server.jar the EULA step downloads.",
                ),
            )
        }
        if (id !in enchantments) {
            throw CommandSyntaxException(
                Component.translatable(
                    "argument.resource.not_found",
                    Component.text(id.toString()),
                    Component.text("minecraft:enchantment"),
                ),
            )
        }
        val targets = context.get<List<PlayerSession>>("targets")
        var enchanted = 0
        for (target in targets) {
            val slot = PlayerInventory.HOTBAR_START + target.inventory.heldSlot
            val held = target.inventory[slot]
            if (held == null) {
                throw CommandSyntaxException(
                    Component.translatable(
                        "commands.enchant.failed.itemless",
                        Component.text(target.name),
                    ),
                )
            }
            val components = NbtCompound()
            for (key in held.components.keys) held.components[key]?.let { components[key] = it }
            val existing = components[PlayerInventory.ENCHANTMENTS_COMPONENT] as? NbtCompound ?: NbtCompound()
            val updated = NbtCompound()
            for (key in existing.keys) existing[key]?.let { updated[key] = it }
            if (level == 0) updated.remove(id.toString()) else updated.putInt(id.toString(), level)
            if (updated.keys.isEmpty()) {
                components.remove(PlayerInventory.ENCHANTMENTS_COMPONENT)
            } else {
                components[PlayerInventory.ENCHANTMENTS_COMPONENT] = updated
            }
            target.inventory[slot] = held.copy(components = components)
            target.connection.sendInventoryAll()
            enchanted++
        }
        if (enchanted == 0) throw CommandSyntaxException(Component.translatable("commands.enchant.failed"))
        context.success(
            if (targets.size == 1) {
                Component.translatable(
                    "commands.enchant.success.single",
                    Component.text(id.toString()),
                    Component.text(targets.single().name),
                )
            } else {
                Component.translatable(
                    "commands.enchant.success.multiple",
                    Component.text(id.toString()),
                    Component.text(targets.size.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return enchanted
    }

    private companion object {
        /** parity: vanilla's own cap on the level `/enchant` accepts. */
        const val MAX_LEVEL = 255
    }
}

/**
 * parity: `DialogCommand`.
 *
 * A dialog is a registry entry, so this only works once the `minecraft:dialog` registry has gone
 * out (see `RealDialogs`, which reads it from the operator's own `server.jar`). Without it the
 * command says so rather than naming an id no client knows.
 */
public class DialogCommand(
    private val dialogs: RealDialogs,
) : Command(name = "dialog", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("show") {
            argument("targets", PlayersArgument(single = false)) {
                argument("dialog", ResourceLocationArgument) { executes { context -> show(context) } }
            }
        }
        root.literal("clear") {
            argument("targets", PlayersArgument(single = false)) { executes { context -> clear(context) } }
        }
    }

    private fun show(context: CommandContext): Int {
        val id = context.get<Identifier>("dialog")
        if (dialogs.size == 0) {
            throw CommandSyntaxException(
                Component.text(
                    "No dialog registry has been sent to clients, so '$id' would not resolve on one. " +
                        "It is read from the server.jar the EULA step downloads.",
                ),
            )
        }
        val dialogId =
            dialogs.protocolIdOf(id)
                ?: throw CommandSyntaxException(
                    Component.translatable(
                        "argument.resource.not_found",
                        Component.text(id.toString()),
                        Component.text("minecraft:dialog"),
                    ),
                )
        val targets = context.get<List<PlayerSession>>("targets")
        val packet = ShowDialogPacket(dialogId)
        for (target in targets) {
            target.connection.sendPlayPacket(ShowDialogPacket.ID) { packet.write(it) }
        }
        context.success(
            if (targets.size == 1) {
                Component.translatable("commands.dialog.show.single", Component.text(targets.single().name))
            } else {
                Component.translatable("commands.dialog.show.multiple", Component.text(targets.size.toString()))
            },
            broadcastToAdmins = true,
        )
        return targets.size
    }

    private fun clear(context: CommandContext): Int {
        val targets = context.get<List<PlayerSession>>("targets")
        for (target in targets) {
            target.connection.sendPlayPacket(ClearDialogPacket.ID) { ClearDialogPacket.write(it) }
        }
        context.success(
            if (targets.size == 1) {
                Component.translatable("commands.dialog.clear.single", Component.text(targets.single().name))
            } else {
                Component.translatable("commands.dialog.clear.multiple", Component.text(targets.size.toString()))
            },
            broadcastToAdmins = true,
        )
        return targets.size
    }
}

/**
 * parity: `TestCommand`, for the kind of test this server can actually run.
 *
 * A `minecraft:function` test is a function that fails by failing, which is the kind the vanilla
 * jar itself ships and the only kind a datapack can write without the block-assertion API. A test
 * of any other type is reported as skipped rather than passed, because claiming a pass for
 * something that never ran is the one answer a test framework must never give.
 */
public class TestCommand(
    private val instances: RealTestInstances,
    private val runner: FunctionRunner,
) : Command(name = "test", permissionLevel = PermissionLevel.GAMEMASTERS) {
    /** Which tests failed last time, so `runfailed` has something to run. */
    private val failed = mutableSetOf<Identifier>()

    override fun build(root: CommandNodeBuilder) {
        root.literal("runall") { executes { context -> runAll(context, instances.all) } }
        root.literal("runthese") { executes { context -> runAll(context, instances.all) } }
        root.literal("runfailed") {
            executes { context -> runAll(context, instances.all.filter { it.id in failed }) }
        }
        root.literal("clearall") {
            executes { context ->
                val cleared = failed.size
                failed.clear()
                context.success(
                    Component.translatable("commands.test.clear.success", Component.text(cleared.toString())),
                )
                cleared
            }
        }
        root.literal("run") {
            argument("name", ResourceLocationArgument) {
                executes { context ->
                    val id = context.get<Identifier>("name")
                    val instance =
                        instances[id]
                            ?: throw CommandSyntaxException(
                                Component.translatable(
                                    "commands.test.error.test_instance_not_found",
                                ),
                            )
                    runAll(context, listOf(instance))
                }
            }
        }
    }

    private fun runAll(
        context: CommandContext,
        selected: List<TestInstance>,
    ): Int {
        if (selected.isEmpty()) {
            context.success(Component.translatable("commands.test.no_tests"))
            return 0
        }
        var passed = 0
        var skipped = 0
        for (instance in selected) {
            if (!instance.isFunctionTest) {
                skipped++
                context.source.sendSystemMessage(
                    Component.text("  ${instance.id}: skipped, this build runs function tests only"),
                )
                continue
            }
            val outcome =
                runCatching {
                    runner.run(context.dispatcher, context.source, requireNotNull(instance.function))
                }
            if (outcome.isSuccess) {
                passed++
                failed -= instance.id
                context.source.sendSystemMessage(Component.text("  ${instance.id}: passed"))
            } else {
                failed += instance.id
                context.source.sendSystemMessage(
                    Component.text("  ${instance.id}: failed (${outcome.exceptionOrNull()?.message.orEmpty()})"),
                )
            }
        }
        val ran = selected.size - skipped
        context.success(
            Component.text("Ran $ran test(s): $passed passed, ${ran - passed} failed, $skipped skipped."),
            broadcastToAdmins = true,
        )
        return passed
    }
}
