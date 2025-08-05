package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.assets.ServerJarAssets
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.FloatArgument
import org.basaltmc.server.runtime.command.HexColorArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.RegistryArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.TeamColorArgument
import org.basaltmc.server.runtime.command.TemplateMirrorArgument
import org.basaltmc.server.runtime.command.TemplateRotationArgument
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.runtime.world.RealDialogs
import org.basaltmc.server.runtime.world.RealEnchantments
import org.basaltmc.server.runtime.world.RealTestInstances
import org.basaltmc.server.runtime.world.StructureTemplates
import org.basaltmc.server.runtime.world.TestInstance
import org.basaltmc.server.world.structure.PlacedStructurePiece
import org.basaltmc.server.world.structure.StructureBoundingBox
import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructurePos
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.server.world.terrain.VillageStructureGenerator
import org.basaltmc.server.world.terrain.decoration.PlacedFeatures
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.ClearDialogPacket
import org.basaltmc.wire.protocol.play.ShowDialogPacket
import org.basaltmc.wire.protocol.play.TeamColor
import org.basaltmc.wire.protocol.play.TrackedWaypointPacket
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor
import kotlin.random.Random
import kotlin.uuid.ExperimentalUuidApi

/**
 * The locator-bar icon of an entity. parity: `WaypointCommand`.
 *
 * Every change is pushed with `tracked_waypoint`, so the icon really appears on the bar: the
 * waypoint is the positioned kind, identified by the entity's own UUID, which is how vanilla tracks
 * a player's.
 */
public class WaypointCommand(
    private val serverJar: ServerJarAssets? = null,
) : Command(name = "waypoint", permissionLevel = PermissionLevel.GAMEMASTERS) {
    /** Style and colour per entity id, which is what `list` reports and `modify` changes. */
    private val icons = mutableMapOf<Int, Icon>()

    private class Icon(
        var style: Identifier = Identifier.vanilla("default"),
        var color: Int? = null,
    )

    override fun build(root: CommandNodeBuilder) {
        root.literal("list") { executes { context -> list(context) } }
        root.literal("modify") {
            argument("waypoint", EntitiesArgument(single = true)) {
                literal("color") {
                    literal("reset") { executes { context -> resetColor(context) } }
                    literal("hex") {
                        argument("color", HexColorArgument) { executes { context -> setHexColor(context) } }
                    }
                    argument("color", TeamColorArgument) {
                        executes { context -> setNamedColor(context, context.get("color")) }
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
        val target = context.get<List<CommandTarget>>("waypoint").single()
        return icons.getOrPut(target.entityId) { Icon() }
    }

    /** Sends the target's waypoint to everyone in its world, which is what draws the bar entry. */
    @OptIn(ExperimentalUuidApi::class)
    private fun broadcast(context: CommandContext) {
        val target = context.get<List<CommandTarget>>("waypoint").single()
        val icon = icons[target.entityId] ?: return
        val uuid =
            when (target) {
                is CommandTarget.Player -> target.session.uuid
                is CommandTarget.Entity -> target.entity.uuid
                is CommandTarget.Item -> target.item.uuid
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
        val value = context.get<Int>("color")
        iconOf(context).color = value
        broadcast(context)
        context.success(
            Component.translatable("commands.waypoint.modify.color", Component.text(value.toString(16))),
        )
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
            argument("feature", RegistryArgument.key("minecraft:worldgen/placed_feature")) {
                suggestsFromServer { context -> context.matchingIds(PlacedFeatures.ids) }
                executes { context -> placeFeature(context, null) }
                argument("pos", BlockPosArgument) {
                    executes { context -> placeFeature(context, context.get("pos")) }
                }
            }
        }
        root.literal("structure") {
            argument("structure", RegistryArgument.key("minecraft:worldgen/structure")) {
                suggestsFromServer { context -> context.matchingIds(VillageStructureGenerator.STRUCTURE_IDS) }
                executes { context -> placeStructure(context, null) }
                argument("pos", BlockPosArgument) {
                    executes { context -> placeStructure(context, context.get("pos")) }
                }
            }
        }
        root.literal("jigsaw") {
            argument("pool", RegistryArgument.key("minecraft:worldgen/template_pool")) {
                suggestsFromServer { context -> context.matchingIds(templates.poolIds()) }
                argument("target", ResourceLocationArgument) {
                    argument("max_depth", IntegerArgument(min = 1, max = MAX_JIGSAW_DEPTH)) {
                        executes { context -> placeJigsaw(context, null) }
                        argument("position", BlockPosArgument) {
                            executes { context -> placeJigsaw(context, context.get("position")) }
                        }
                    }
                }
            }
        }
        root.literal("template") {
            argument("template", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(templates.templateIds()) }
                executes { context -> place(context, null, StructureRotation.NONE, StructureMirror.NONE) }
                argument("pos", BlockPosArgument) {
                    executes { context ->
                        place(context, context.get("pos"), StructureRotation.NONE, StructureMirror.NONE)
                    }
                    argument("rotation", TemplateRotationArgument) {
                        executes { context ->
                            place(context, context.get("pos"), context.get("rotation"), StructureMirror.NONE)
                        }
                        argument("mirror", TemplateMirrorArgument) {
                            executes { context ->
                                place(context, context.get("pos"), context.get("rotation"), context.get("mirror"))
                            }
                            // parity: integrity and its seed decide which blocks a template drops
                            // on the way in. This build places every block, so they are accepted
                            // and reported rather than silently changing nothing.
                            argument("integrity", FloatArgument(min = 0f, max = 1f)) {
                                executes { context -> placeWhole(context) }
                                argument("seed", IntegerArgument()) {
                                    executes { context -> placeWhole(context) }
                                    literal("strict") { executes { context -> placeWhole(context) } }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /** Every block of the template, whatever integrity was asked for. */
    private fun placeWhole(context: CommandContext): Int {
        val integrity = context.get<Float>("integrity")
        if (integrity < 1f) {
            context.source.sendSystemMessage(
                Component.text("Integrity is not modelled yet: the whole template goes in.", TextColor.GRAY),
            )
        }
        return place(context, context.get("pos"), context.get("rotation"), context.get("mirror"))
    }

    private fun place(
        context: CommandContext,
        coordinates: BlockCoordinates?,
        rotation: StructureRotation,
        mirror: StructureMirror,
    ): Int {
        val id = context.get<Identifier>("template")
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
        val id = context.get<Identifier>("feature")
        val world = context.world()
        val placement = PlacedFeatures[id]
        val contextual = PlacedFeatures.contextual(id)
        if (placement == null && contextual == null) {
            throw CommandSyntaxException(
                Component.text(
                    "'$id' has no positional body in this build, so generation can only run it per chunk. " +
                        "Placeable by id: ${PlacedFeatures.ids.sortedBy { it.path }.joinToString(", ")}.",
                ),
            )
        }
        val (x, y, z) = resolveOrigin(context, coordinates)
        val chunk =
            world.state.chunkIfLoaded(x shr 4, z shr 4)
                ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))

        val random = WorldgenRandom(XoroshiroRandom(Random.nextLong()))
        if (placement != null) {
            placement.place(chunk, random, x and 15, y, z and 15)
        } else {
            // Features that need more than a chunk: the End's own, fossils, the freeze pass and
            // template-backed sulfur roots. Refuse instead of half-placing one without its context.
            val ran =
                checkNotNull(contextual).place(
                    chunk,
                    random,
                    x and 15,
                    y,
                    z and 15,
                    PlacedFeatures.FeatureContext(
                        seed = world.metadata.seed,
                        surface = world.surfaceEnvironment,
                        templates = { templateId -> templates[templateId] },
                        blockStateAt = { blockX, blockY, blockZ ->
                            world.state
                                .chunkIfLoaded(blockX shr 4, blockZ shr 4)
                                ?.takeIf { blockY in it.verticalBounds.minY..it.verticalBounds.maxY }
                                ?.getBlockStateId(blockX and 15, blockY, blockZ and 15)
                                ?: Blocks.AIR.stateId
                        },
                    ),
                )
            if (!ran) {
                throw CommandSyntaxException(
                    Component.text(
                        "'$id' needs something world '${world.name}' does not have (its noise router, its " +
                            "surface rules or the structure files from a downloaded server.jar).",
                    ),
                )
            }
        }
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

    /**
     * parity: `PlaceCommand.placeStructure`, which generates the structure where it was told and
     * skips the placement grid and the biome check that decide where one appears on its own.
     */
    private fun placeStructure(
        context: CommandContext,
        coordinates: BlockCoordinates?,
    ): Int {
        val id = context.get<Identifier>("structure")
        val world = context.world()
        val villages =
            world.villageGenerator
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.place.structure.invalid", Component.text(id.toString())),
                )
        if (id !in villages.structureIds) {
            throw CommandSyntaxException(
                Component.text(
                    "'$id' is not a structure this build generates. Available: " +
                        villages.structureIds.joinToString(", ") + ". `/place template` places a structure file.",
                ),
            )
        }
        val (x, y, z) = resolveOrigin(context, coordinates)
        val pieces =
            villages.assembleAt(id, x shr 4, z shr 4, world.metadata.seed)
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.place.structure.failed", Component.text(id.toString())),
                )
        writePieces(context, world, pieces, villages)
        context.success(
            Component.translatable(
                "commands.place.structure.success",
                Component.text(id.toString()),
                Component.text(x.toString()),
                Component.text(y.toString()),
                Component.text(z.toString()),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }

    /** Where a `/place` acts: the position it was given, or the whole-block position of the runner. */
    private fun resolveOrigin(
        context: CommandContext,
        coordinates: BlockCoordinates?,
    ): Triple<Int, Int, Int> =
        coordinates?.resolve(context.source.position) ?: context.source.position.let { (x, y, z) ->
            Triple(kotlin.math.floor(x).toInt(), kotlin.math.floor(y).toInt(), kotlin.math.floor(z).toInt())
        }

    /** Writes an assembly into every chunk it covers, and re-sends them. Every chunk has to be loaded. */
    private fun writePieces(
        context: CommandContext,
        world: LoadedWorld,
        pieces: List<PlacedStructurePiece>,
        villages: VillageStructureGenerator,
    ) {
        var minX = Int.MAX_VALUE
        var minZ = Int.MAX_VALUE
        var maxX = Int.MIN_VALUE
        var maxZ = Int.MIN_VALUE
        for (piece in pieces) {
            minX = minOf(minX, piece.boundingBox.minX)
            minZ = minOf(minZ, piece.boundingBox.minZ)
            maxX = maxOf(maxX, piece.boundingBox.maxX)
            maxZ = maxOf(maxZ, piece.boundingBox.maxZ)
        }
        for (chunkX in (minX shr 4)..(maxX shr 4)) {
            for (chunkZ in (minZ shr 4)..(maxZ shr 4)) {
                val chunk =
                    world.state.chunkIfLoaded(chunkX, chunkZ)
                        ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
                villages.placeInto(chunk, pieces, world.metadata.seed)
            }
        }
        context.environment.syncChunks(world, minX, minZ, maxX, maxZ)
    }

    /** parity: `PlaceCommand.placeJigsaw`, which grows a template pool at a position with no structure around it. */
    private fun placeJigsaw(
        context: CommandContext,
        coordinates: BlockCoordinates?,
    ): Int {
        val pool = context.get<Identifier>("pool")
        val world = context.world()
        val villages =
            world.villageGenerator
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.place.jigsaw.invalid", Component.text(pool.toString())),
                )
        if (!villages.hasPool(pool.toString())) {
            throw CommandSyntaxException(
                Component.translatable("commands.place.jigsaw.invalid", Component.text(pool.toString())),
            )
        }
        val (x, y, z) = resolveOrigin(context, coordinates)
        val pieces =
            villages.assemblePool(
                pool.toString(),
                x shr 4,
                z shr 4,
                context.get("max_depth"),
                world.metadata.seed,
            )
        if (pieces.isEmpty()) {
            throw CommandSyntaxException(
                Component.translatable("commands.place.jigsaw.failed", Component.text(pool.toString())),
            )
        }
        writePieces(context, world, pieces, villages)
        context.success(
            Component.translatable(
                "commands.place.jigsaw.success",
                Component.text(x.toString()),
                Component.text(y.toString()),
                Component.text(z.toString()),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }

    private companion object {
        /** parity: vanilla's own `/place jigsaw` cap. */
        const val MAX_JIGSAW_DEPTH = 20
    }
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
            argument("enchantment", RegistryArgument.resource("minecraft:enchantment")) {
                suggestsFromServer { context -> context.matchingIds(enchantments.ids) }
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
                // No completions of our own: `minecraft:dialog` is a synced registry, so the client
                // completes it from the same list this server sent it.
                argument("dialog", RegistryArgument.DIALOG) { executes { context -> show(context) } }
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
        // parity: the whole tree, so a datapack that drives tests parses here too. The arguments
        // that lay tests out in the world (rotation, rows, radius) are read and ignored: there are
        // no test structures in this build, only function tests.
        root.literal("runall") { executes { context -> runAll(context, instances.all) } }
        for (name in listOf("runthese", "runthat", "runclosest")) {
            root.literal(name) {
                executes { context -> runAll(context, instances.all) }
                argument("numberOfTimes", IntegerArgument(min = 0)) {
                    executes { context -> runAll(context, instances.all) }
                    argument("untilFailed", BoolArgument) { executes { context -> runAll(context, instances.all) } }
                }
            }
        }
        root.literal("runfailed") {
            executes { context -> runFailed(context) }
            argument("onlyRequiredTests", BoolArgument) {
                executes { context -> runFailed(context) }
                repeats { context -> runFailed(context) }
            }
            repeats { context -> runFailed(context) }
        }
        root.literal("run") {
            argument("tests", RegistryArgument.selector("minecraft:test_instance")) {
                suggestsFromServer { context -> context.matchingIds(instances.all.map { it.id }) }
                executes { context -> runNamed(context) }
                repeats { context -> runNamed(context) }
            }
        }
        root.literal("runmultiple") {
            argument("tests", RegistryArgument.selector("minecraft:test_instance")) {
                suggestsFromServer { context -> context.matchingIds(instances.all.map { it.id }) }
                executes { context -> runNamed(context) }
                argument("amount", IntegerArgument(min = 0)) { executes { context -> runNamed(context) } }
            }
        }
        root.literal("verify") {
            argument("tests", RegistryArgument.selector("minecraft:test_instance")) {
                suggestsFromServer { context -> context.matchingIds(instances.all.map { it.id }) }
                executes { context -> runNamed(context) }
            }
        }
        root.literal("clearall") {
            executes { context -> clear(context) }
            argument("radius", IntegerArgument(min = 0)) { executes { context -> clear(context) } }
        }
        for (name in listOf("clearthat", "clearthese")) {
            root.literal(name) { executes { context -> clear(context) } }
        }
        for (name in listOf("resetclosest", "resetthat", "resetthese")) {
            root.literal(name) { executes { context -> nothingPlaced(context) } }
        }
        root.literal("locate") {
            argument("tests", RegistryArgument.selector("minecraft:test_instance")) {
                suggestsFromServer { context -> context.matchingIds(instances.all.map { it.id }) }
                executes { context -> nothingPlaced(context) }
            }
        }
        root.literal("create") {
            argument("id", ResourceLocationArgument) {
                executes { context -> noStructures(context) }
                argument("width", IntegerArgument(min = 0)) {
                    executes { context -> noStructures(context) }
                    argument("height", IntegerArgument(min = 0)) {
                        argument("depth", IntegerArgument(min = 0)) { executes { context -> noStructures(context) } }
                    }
                }
            }
        }
        root.literal("pos") {
            executes { context -> reportPosition(context, "pos") }
            argument("var", WordArgument) { executes { context -> reportPosition(context, context.get("var")) } }
        }
        root.literal("stop") { executes { context -> stop(context) } }
    }

    /** The `<numberOfTimes> [<untilFailed>] [<rotationSteps>] [<testsPerRow>]` tail several forms share. */
    private fun CommandNodeBuilder.repeats(run: (CommandContext) -> Int) {
        argument("numberOfTimes", IntegerArgument(min = 0)) {
            executes { context -> run(context) }
            argument("untilFailed", BoolArgument) {
                executes { context -> run(context) }
                argument("rotationSteps", IntegerArgument(min = 0)) {
                    executes { context -> run(context) }
                    argument("testsPerRow", IntegerArgument(min = 1)) { executes { context -> run(context) } }
                }
            }
        }
    }

    private fun runFailed(context: CommandContext): Int = runAll(context, instances.all.filter { it.id in failed })

    private fun runNamed(context: CommandContext): Int {
        val id = context.get<Identifier>("tests")
        val instance =
            instances[id]
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.test.error.test_instance_not_found"),
                )
        return runAll(context, listOf(instance))
    }

    private fun clear(context: CommandContext): Int {
        val cleared = failed.size
        failed.clear()
        context.success(Component.translatable("commands.test.clear.success", Component.text(cleared.toString())))
        return cleared
    }

    /** Nothing is laid out in the world, so there is nothing to reset, locate or clear there. */
    private fun nothingPlaced(context: CommandContext): Int {
        context.success(Component.translatable("commands.test.error.no_test_instances"))
        return 0
    }

    private fun noStructures(context: CommandContext): Int =
        throw CommandSyntaxException(
            Component.text("Test structures are not modelled in this build: only function tests run."),
        )

    /** parity: `/test pos`, which prints the position as a line to paste into a test. */
    private fun reportPosition(
        context: CommandContext,
        variable: String,
    ): Int {
        val (x, y, z) = context.source.position
        context.success(
            Component.text("final BlockPos $variable = new BlockPos(${x.toInt()}, ${y.toInt()}, ${z.toInt()});"),
        )
        return 1
    }

    private fun stop(context: CommandContext): Int {
        context.success(Component.translatable("commands.test.no_tests"))
        return 0
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
