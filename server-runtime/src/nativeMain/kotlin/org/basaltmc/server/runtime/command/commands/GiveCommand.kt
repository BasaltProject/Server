package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.ItemArgument
import org.basaltmc.server.runtime.command.ParsedItem
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.text.Component

/**
 * parity: `GiveCommand`.
 *
 * Every component the registry knows is accepted and stored. The ones this build has no network
 * codec for are reported by name, the way `/summon` reports the NBT keys it ignores: the item is
 * really carrying them, but no client draws them yet.
 */
public class GiveCommand : Command(name = "give", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", PlayersArgument(single = false)) {
            argument("item", ItemArgument) {
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
        parsed: ParsedItem,
        count: Int,
        context: org.basaltmc.server.runtime.command.CommandContext,
    ): Int {
        val item =
            Items.REGISTRY.byIdentifier(parsed.id)
                ?: throw CommandSyntaxException(
                    Component.translatable("argument.item.id.invalid", Component.text(parsed.id.toString())),
                )

        val notDrawn = ItemComponentCodecs.notDrawn(parsed.components)
        if (notDrawn.isNotEmpty()) {
            context.source.sendSystemMessage(
                Component.text(
                    "The item carries ${notDrawn.joinToString(", ")}, which no client can draw yet: " +
                        "it is stored and saved, but the tooltip will not show it.",
                ),
            )
        }

        for (target in targets) {
            val leftOver = target.inventory.add(StoredItemStack(item.id.toString(), count, parsed.components))
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
