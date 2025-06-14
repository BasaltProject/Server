package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.ComponentArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.BossBarColor
import org.basaltmc.wire.protocol.play.BossBarOverlay
import org.basaltmc.wire.protocol.play.BossEventPacket
import org.basaltmc.wire.text.Component
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** One `/bossbar` bar: what it says, how full it is, and who can see it. */
@OptIn(ExperimentalUuidApi::class)
public class CustomBossBar(
    public val id: Identifier,
    public var name: Component,
) {
    public val uuid: Uuid = Uuid.random()
    public var color: BossBarColor = BossBarColor.WHITE
    public var overlay: BossBarOverlay = BossBarOverlay.PROGRESS
    public var value: Int = 0
    public var max: Int = 100
    public var visible: Boolean = true

    /** Player names rather than sessions: a bar outlives any one connection, as vanilla's does. */
    public val players: MutableSet<String> = mutableSetOf()

    public val progress: Float get() = if (max == 0) 0f else (value.toFloat() / max).coerceIn(0f, 1f)

    public fun addOperation(): BossEventPacket.Operation.Add =
        BossEventPacket.Operation.Add(name, progress, color, overlay)
}

/**
 * Bars by id, and the packets that keep clients in step with them.
 *
 * Held by the server rather than by the command, so a bar survives a `/reload` and so the join path
 * can show a returning player the bars they belong to. They are not persisted across a restart yet,
 * which vanilla does in `level.dat`; stated rather than silently lost.
 */
@OptIn(ExperimentalUuidApi::class)
public class BossBarRegistry {
    private val bars = linkedMapOf<Identifier, CustomBossBar>()

    public val all: List<CustomBossBar> get() = bars.values.toList()

    public operator fun get(id: Identifier): CustomBossBar? = bars[id]

    public fun add(bar: CustomBossBar) {
        bars[bar.id] = bar
    }

    public fun remove(id: Identifier): CustomBossBar? = bars.remove(id)

    /** Everything [player] should see the moment they join, which is every visible bar naming them. */
    public fun barsFor(player: PlayerSession): List<CustomBossBar> =
        bars.values.filter { it.visible && player.name in it.players }
}

/** parity: `BossBarCommands`. */
@OptIn(ExperimentalUuidApi::class)
public class BossBarCommand(
    private val registry: BossBarRegistry,
) : Command(name = "bossbar", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("add") {
            argument("id", ResourceLocationArgument) {
                argument("name", ComponentArgument) {
                    executes { context -> add(context) }
                }
            }
        }
        root.literal("remove") {
            argument("id", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(registry.all.map { it.id }) }
                executes { context -> remove(context) }
            }
        }
        root.literal("list") { executes { context -> list(context) } }
        root.literal("get") {
            argument("id", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(registry.all.map { it.id }) }
                for (what in listOf("value", "max", "visible", "players")) {
                    literal(what) { executes { context -> get(context, what) } }
                }
            }
        }
        root.literal("set") {
            argument("id", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(registry.all.map { it.id }) }
                literal("name") {
                    argument("name", ComponentArgument) { executes { context -> setName(context) } }
                }
                literal("color") {
                    for (color in BossBarColor.entries) {
                        literal(color.name.lowercase()) { executes { context -> setColor(context, color) } }
                    }
                }
                literal("style") {
                    for (overlay in BossBarOverlay.entries) {
                        literal(overlay.commandName) { executes { context -> setStyle(context, overlay) } }
                    }
                }
                literal("value") {
                    argument("value", IntegerArgument(min = 0)) { executes { context -> setValue(context) } }
                }
                literal("max") {
                    argument("max", IntegerArgument(min = 1)) { executes { context -> setMax(context) } }
                }
                literal("visible") {
                    argument("visible", BoolArgument) {
                        executes { context -> setVisible(context, context.get("visible")) }
                    }
                }
                literal("players") {
                    executes { context -> setPlayers(context, emptyList()) }
                    argument("targets", PlayersArgument(single = false)) {
                        executes { context -> setPlayers(context, context.get("targets")) }
                    }
                }
            }
        }
    }

    private fun barOf(context: CommandContext): CustomBossBar {
        val id = context.get<Identifier>("id")
        return registry[id]
            ?: throw CommandSyntaxException(
                Component.translatable("commands.bossbar.unknown", Component.text(id.toString())),
            )
    }

    private fun add(context: CommandContext): Int {
        val id = context.get<Identifier>("id")
        if (registry[id] != null) {
            throw CommandSyntaxException(
                Component.translatable("commands.bossbar.create.failed", Component.text(id.toString())),
            )
        }
        val bar = CustomBossBar(id, context.get("name"))
        registry.add(bar)
        context.success(Component.translatable("commands.bossbar.create.success", bar.name), broadcastToAdmins = true)
        return registry.all.size
    }

    private fun remove(context: CommandContext): Int {
        val bar = barOf(context)
        for (player in viewers(context, bar)) {
            player.connection.sendPlayPacket(BossEventPacket.ID) {
                BossEventPacket(bar.uuid, BossEventPacket.Operation.Remove).write(it)
            }
        }
        registry.remove(bar.id)
        context.success(Component.translatable("commands.bossbar.remove.success", bar.name), broadcastToAdmins = true)
        return registry.all.size
    }

    private fun list(context: CommandContext): Int {
        val bars = registry.all
        context.success(
            if (bars.isEmpty()) {
                Component.translatable("commands.bossbar.list.bars.none")
            } else {
                Component.translatable(
                    "commands.bossbar.list.bars.some",
                    Component.text(bars.size.toString()),
                    Component.text(bars.joinToString(", ") { it.id.toString() }),
                )
            },
        )
        return bars.size
    }

    private fun get(
        context: CommandContext,
        what: String,
    ): Int {
        val bar = barOf(context)
        return when (what) {
            "value" -> {
                context.success(
                    Component.translatable(
                        "commands.bossbar.get.value",
                        bar.name,
                        Component.text(bar.value.toString()),
                    ),
                )
                bar.value
            }
            "max" -> {
                context.success(
                    Component.translatable("commands.bossbar.get.max", bar.name, Component.text(bar.max.toString())),
                )
                bar.max
            }
            "visible" -> {
                val key =
                    if (bar.visible) {
                        "commands.bossbar.get.visible.visible"
                    } else {
                        "commands.bossbar.get.visible.hidden"
                    }
                context.success(Component.translatable(key, bar.name))
                if (bar.visible) 1 else 0
            }
            else -> {
                context.success(
                    if (bar.players.isEmpty()) {
                        Component.translatable("commands.bossbar.get.players.none", bar.name)
                    } else {
                        Component.translatable(
                            "commands.bossbar.get.players.some",
                            bar.name,
                            Component.text(bar.players.size.toString()),
                            Component.text(bar.players.joinToString(", ")),
                        )
                    },
                )
                bar.players.size
            }
        }
    }

    private fun setName(context: CommandContext): Int {
        val bar = barOf(context)
        val name = context.get<Component>("name")
        if (bar.name == name) throw unchanged("name")
        bar.name = name
        broadcast(context, bar) { BossEventPacket.Operation.UpdateName(bar.name) }
        context.success(Component.translatable("commands.bossbar.set.name.success", bar.name), broadcastToAdmins = true)
        return 0
    }

    private fun setColor(
        context: CommandContext,
        color: BossBarColor,
    ): Int {
        val bar = barOf(context)
        if (bar.color == color) throw unchanged("color")
        bar.color = color
        broadcast(context, bar) { BossEventPacket.Operation.UpdateStyle(bar.color, bar.overlay) }
        context.success(
            Component.translatable("commands.bossbar.set.color.success", bar.name),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun setStyle(
        context: CommandContext,
        overlay: BossBarOverlay,
    ): Int {
        val bar = barOf(context)
        if (bar.overlay == overlay) throw unchanged("style")
        bar.overlay = overlay
        broadcast(context, bar) { BossEventPacket.Operation.UpdateStyle(bar.color, bar.overlay) }
        context.success(
            Component.translatable("commands.bossbar.set.style.success", bar.name),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun setValue(context: CommandContext): Int {
        val bar = barOf(context)
        val value = context.get<Int>("value")
        if (bar.value == value) throw unchanged("value")
        bar.value = value
        broadcast(context, bar) { BossEventPacket.Operation.UpdateProgress(bar.progress) }
        context.success(
            Component.translatable(
                "commands.bossbar.set.value.success",
                bar.name,
                Component.text(value.toString()),
            ),
            broadcastToAdmins = true,
        )
        return value
    }

    private fun setMax(context: CommandContext): Int {
        val bar = barOf(context)
        val max = context.get<Int>("max")
        if (bar.max == max) throw unchanged("max")
        bar.max = max
        broadcast(context, bar) { BossEventPacket.Operation.UpdateProgress(bar.progress) }
        context.success(
            Component.translatable("commands.bossbar.set.max.success", bar.name, Component.text(max.toString())),
            broadcastToAdmins = true,
        )
        return max
    }

    private fun setVisible(
        context: CommandContext,
        visible: Boolean,
    ): Int {
        val bar = barOf(context)
        if (bar.visible == visible) {
            throw CommandSyntaxException(
                Component.translatable(
                    if (visible) {
                        "commands.bossbar.set.visibility.unchanged.visible"
                    } else {
                        "commands.bossbar.set.visibility.unchanged.hidden"
                    },
                ),
            )
        }
        bar.visible = visible
        for (player in viewers(context, bar)) {
            player.connection.sendPlayPacket(BossEventPacket.ID) {
                val operation =
                    if (visible) bar.addOperation() else BossEventPacket.Operation.Remove
                BossEventPacket(bar.uuid, operation).write(it)
            }
        }
        context.success(
            Component.translatable(
                if (visible) {
                    "commands.bossbar.set.visible.success.visible"
                } else {
                    "commands.bossbar.set.visible.success.hidden"
                },
                bar.name,
            ),
            broadcastToAdmins = true,
        )
        return 0
    }

    private fun setPlayers(
        context: CommandContext,
        targets: List<PlayerSession>,
    ): Int {
        val bar = barOf(context)
        val names = targets.map { it.name }.toSet()
        if (bar.players == names) throw unchanged("players")

        val online = context.environment.players.snapshot()
        // Whoever is dropping off the bar loses it now, before the set changes underneath them.
        for (player in online.filter { it.name in bar.players && it.name !in names }) {
            player.connection.sendPlayPacket(BossEventPacket.ID) {
                BossEventPacket(bar.uuid, BossEventPacket.Operation.Remove).write(it)
            }
        }
        val added = targets.filter { it.name !in bar.players }
        bar.players.clear()
        bar.players += names
        if (bar.visible) {
            for (player in added) {
                player.connection.sendPlayPacket(BossEventPacket.ID) {
                    BossEventPacket(bar.uuid, bar.addOperation()).write(it)
                }
            }
        }
        context.success(
            if (names.isEmpty()) {
                Component.translatable("commands.bossbar.set.players.success.none", bar.name)
            } else {
                Component.translatable(
                    "commands.bossbar.set.players.success.some",
                    bar.name,
                    Component.text(names.size.toString()),
                    Component.text(names.joinToString(", ")),
                )
            },
            broadcastToAdmins = true,
        )
        return names.size
    }

    private fun viewers(
        context: CommandContext,
        bar: CustomBossBar,
    ): List<PlayerSession> =
        context.environment.players
            .snapshot()
            .filter { it.name in bar.players }

    private inline fun broadcast(
        context: CommandContext,
        bar: CustomBossBar,
        operation: () -> BossEventPacket.Operation,
    ) {
        if (!bar.visible) return
        val packet = BossEventPacket(bar.uuid, operation())
        for (player in viewers(context, bar)) {
            player.connection.sendPlayPacket(BossEventPacket.ID) { packet.write(it) }
        }
    }

    private fun unchanged(what: String) =
        CommandSyntaxException(Component.translatable("commands.bossbar.set.$what.unchanged"))
}
