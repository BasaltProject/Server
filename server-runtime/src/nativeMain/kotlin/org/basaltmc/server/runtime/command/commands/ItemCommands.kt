package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.ItemArgument
import org.basaltmc.server.runtime.command.ItemSlotArgument
import org.basaltmc.server.runtime.command.ParsedItem
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.text.Component

/**
 * The slot names `/item` takes, mapped to this server's own inventory indices.
 *
 * parity: `SlotArgument`'s names. Only a player's own slots are here, because a container's slots
 * need block entities with inventories, which do not exist yet.
 */
internal object ItemSlots {
    fun resolve(name: String): Int? =
        when {
            name.startsWith("hotbar.") ->
                name.removePrefix("hotbar.").toIntOrNull()?.takeIf { it in 0..8 }?.let {
                    PlayerInventory.HOTBAR_START +
                        it
                }
            name.startsWith("inventory.") ->
                name.removePrefix("inventory.").toIntOrNull()?.takeIf { it in 0..26 }
            name == "weapon" || name == "weapon.mainhand" -> null
            name == "armor.feet" -> ARMOUR_START
            name == "armor.legs" -> ARMOUR_START + 1
            name == "armor.chest" -> ARMOUR_START + 2
            name == "armor.head" -> ARMOUR_START + 3
            name == "weapon.offhand" -> OFFHAND
            else -> null
        }

    /** Every name [resolve] answers, for completions. The numbered ones are listed in full. */
    val names: List<String> =
        List(9) { "hotbar.$it" } + List(27) { "inventory.$it" } +
            listOf("armor.feet", "armor.legs", "armor.chest", "armor.head", "weapon.offhand")

    private const val ARMOUR_START = 36
    private const val OFFHAND = 40
}

/**
 * parity: `ItemCommands`, `replace entity` only.
 *
 * `replace block` and the `copy` forms need container block entities, which this server does not
 * model yet, so they are not declared rather than accepted and ignored.
 */
public class ItemCommand : Command(name = "item", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("replace") {
            literal("entity") {
                argument("targets", PlayersArgument(single = false)) {
                    argument("slot", ItemSlotArgument) {
                        literal("with") {
                            argument("item", ItemArgument) {
                                executes { context -> replace(context, 1) }
                                argument("count", IntegerArgument(min = 1, max = MAX_COUNT)) {
                                    executes { context -> replace(context, context.get("count")) }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun replace(
        context: CommandContext,
        count: Int,
    ): Int {
        val targets = context.get<List<PlayerSession>>("targets")
        val slotName = context.get<String>("slot")
        val slot =
            ItemSlots.resolve(slotName)
                ?: throw CommandSyntaxException(
                    Component.translatable("slot.unknown", Component.text(slotName)),
                )
        val parsed = context.get<ParsedItem>("item")
        val item = parsed.id
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
            target.inventory[slot] = StoredItemStack(item.toString(), count, parsed.components)
            target.connection.sendInventoryAll()
        }
        context.success(
            if (targets.size == 1) {
                Component.translatable(
                    "commands.item.entity.set.success.single",
                    Component.text(targets.single().name),
                    Component.text(item.toString()),
                )
            } else {
                Component.translatable(
                    "commands.item.entity.set.success.multiple",
                    Component.text(targets.size.toString()),
                    Component.text(item.toString()),
                )
            },
            broadcastToAdmins = true,
        )
        return targets.size
    }

    private companion object {
        const val MAX_COUNT = 99
    }
}

/**
 * parity: `LootCommand`, over the source this server can actually roll: a block's own loot table.
 *
 * `loot <table>`, `fish`, `kill` and `mine` need loot tables that are not a block's, an entity to
 * kill, or a tool with components; `insert` needs a container. `give ... mine <pos>` is the form
 * that works end to end here, because breaking a block already rolls exactly this table.
 */
public class LootCommand(
    private val lootTables: RealLootTables,
    private val worldSeed: () -> Long,
) : Command(name = "loot", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("give") {
            argument("players", PlayersArgument(single = false)) {
                literal("mine") {
                    argument("pos", BlockPosArgument) { executes { context -> give(context) } }
                }
            }
        }
    }

    private fun give(context: CommandContext): Int {
        val world = context.world()
        val players = context.get<List<PlayerSession>>("players")
        val (x, y, z) = context.get<BlockCoordinates>("pos").resolve(context.source.position)
        val stateId =
            world.state.getBlockState(x, y, z)
                ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
        val block =
            Blocks.byStateId(stateId)?.block
                ?: throw CommandSyntaxException(Component.text("Nothing is at that position."))
        val table =
            lootTables.forBlock(block)
                ?: throw CommandSyntaxException(
                    Component.text("$block has no loot table this build can roll, so it would drop nothing."),
                )

        val random = LegacyRandom(worldSeed() xor ((x.toLong() shl 32) or z.toLong()))
        val stacks = table.generate(random)
        if (stacks.isEmpty()) {
            context.success(
                Component.translatable("commands.drop.no_loot_table.block", Component.text(block.toString())),
            )
            return 0
        }
        for (player in players) {
            for (stack in stacks) {
                player.inventory.add(StoredItemStack(stack.item.toString(), stack.count))
            }
            player.connection.sendInventoryAll()
        }
        context.success(
            Component.translatable(
                "commands.give.success.single",
                Component.text(stacks.sumOf { it.count }.toString()),
                Component.text(block.toString()),
                Component.text(if (players.size == 1) players.single().name else players.size.toString()),
            ),
            broadcastToAdmins = true,
        )
        return stacks.size * players.size
    }
}
