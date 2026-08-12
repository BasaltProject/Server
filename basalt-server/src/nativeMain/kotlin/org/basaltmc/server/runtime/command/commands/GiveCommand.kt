package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component

/**
 * parity: `GiveCommand`, minus the data components.
 *
 * Vanilla's takes an item *predicate* with components attached (`/give @s diamond_sword[enchantments=...]`).
 * This takes a plain item id, because a stack with components cannot be sent to the client yet:
 * writing them needs a codec per component type. Asking for one is refused with that reason rather
 * than silently handing over a plain sword.
 */
public class GiveCommand : Command(name = "give", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", PlayersArgument(single = false)) {
            argument("item", WordArgument) {
                executes { context -> give(context.get("targets"), context.get("item"), 1, context) }
                argument("count", IntegerArgument(min = 1, max = MAX_COUNT)) {
                    executes { context ->
                        give(context.get("targets"), context.get("item"), context.get("count"), context)
                    }
                }
            }
        }
    }

    private fun give(
        targets: List<PlayerSession>,
        rawItem: String,
        count: Int,
        context: org.basaltmc.server.runtime.command.CommandContext,
    ): Int {
        if ('[' in rawItem) {
            throw CommandSyntaxException(
                Component.text(
                    "Item components are not supported yet: this build can only send plain items, " +
                        "so '$rawItem' would arrive as something other than what you asked for.",
                ),
            )
        }
        val identifier = Identifier.parse(rawItem)
        val item =
            Items.REGISTRY.byIdentifier(identifier)
                ?: throw CommandSyntaxException(
                    Component.translatable("argument.item.id.invalid", Component.text(rawItem)),
                )

        for (target in targets) {
            val leftOver = target.inventory.add(StoredItemStack(item.id.toString(), count))
            // Nothing is dropped on the floor, because there are no item entities yet. Saying so
            // beats a silent disappearance the operator only notices later.
            if (leftOver > 0) {
                context.source.sendSystemMessage(
                    Component.text(
                        "${target.name}'s inventory was full: $leftOver of $count did not fit and were lost, " +
                            "since dropped items do not exist yet.",
                    ),
                )
            }
            target.connection.sendInventoryAll()
        }

        val what = "$count ${item.id}"
        context.success(
            if (targets.size == 1) {
                Component.translatable(
                    "commands.give.success.single",
                    Component.text("$count"),
                    Component.text(item.id.toString()),
                    Component.text(targets.single().name),
                )
            } else {
                Component.text("Gave $what to ${targets.size} players.")
            },
            broadcastToAdmins = true,
        )
        return targets.size
    }

    private companion object {
        /** Vanilla's own cap on `/give`, which exists so a typo cannot ask for two billion items. */
        const val MAX_COUNT = 6400
    }
}
