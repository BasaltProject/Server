package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.BlockCoordinates
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.CommandTarget
import org.basaltmc.server.runtime.command.Coordinates
import org.basaltmc.server.runtime.command.EntitiesArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.ItemArgument
import org.basaltmc.server.runtime.command.ItemSlotArgument
import org.basaltmc.server.runtime.command.LootModifierArgument
import org.basaltmc.server.runtime.command.LootTableArgument
import org.basaltmc.server.runtime.command.ParsedItem
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.Vec3Argument
import org.basaltmc.server.runtime.command.world
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.RealLootTables
import org.basaltmc.server.runtime.world.WorldState
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.text.Component
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

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
                        sources { context -> copyInto(context, entityTargets = true) }
                    }
                }
            }
            literal("block") {
                argument("pos", BlockPosArgument) {
                    argument("slot", ItemSlotArgument) {
                        literal("with") {
                            argument("item", ItemArgument) {
                                executes { context -> replaceInBlock(context, 1) }
                                argument("count", IntegerArgument(min = 1, max = MAX_COUNT)) {
                                    executes { context -> replaceInBlock(context, context.get("count")) }
                                }
                            }
                        }
                        sources { context -> copyInto(context, entityTargets = false) }
                    }
                }
            }
        }
        root.literal("modify") {
            literal("entity") {
                argument("targets", PlayersArgument(single = false)) {
                    argument("slot", ItemSlotArgument) {
                        argument("modifier", LootModifierArgument) {
                            executes { context -> noModifiers(context) }
                        }
                    }
                }
            }
            literal("block") {
                argument("pos", BlockPosArgument) {
                    argument("slot", ItemSlotArgument) {
                        argument("modifier", LootModifierArgument) {
                            executes { context -> noModifiers(context) }
                        }
                    }
                }
            }
        }
    }

    /** `from block|entity <source> <sourceSlot> [<modifier>]`, shared by both destinations. */
    private fun CommandNodeBuilder.sources(run: (CommandContext) -> Int) {
        literal("from") {
            literal("block") {
                argument("source", BlockPosArgument) {
                    argument("sourceSlot", ItemSlotArgument) {
                        executes { context -> run(context) }
                        argument("modifier", LootModifierArgument) {
                            executes { context -> noModifiers(context) }
                        }
                    }
                }
            }
            literal("entity") {
                argument("source", PlayersArgument(single = true)) {
                    argument("sourceSlot", ItemSlotArgument) {
                        executes { context -> run(context) }
                        argument("modifier", LootModifierArgument) {
                            executes { context -> noModifiers(context) }
                        }
                    }
                }
            }
        }
    }

    /**
     * parity: `/item ... <modifier>` runs a loot function over the stack.
     *
     * There are no item modifiers in this build, so the form is declared and refused by name rather
     * than quietly copying the stack unchanged.
     */
    private fun noModifiers(context: CommandContext): Int {
        val modifier = context.get<Identifier>("modifier").toString()
        throw CommandSyntaxException(
            Component.text("Item modifiers are not loaded in this build, so '$modifier' cannot be applied."),
        )
    }

    /** Copies a stack out of a block or a player and into the destination this form names. */
    private fun copyInto(
        context: CommandContext,
        entityTargets: Boolean,
    ): Int {
        val source =
            readSource(context)
                ?: throw CommandSyntaxException(Component.translatable("commands.item.source.no_such_slot"))
        val slot = slotOf(context, "slot")
        return if (entityTargets) {
            val targets = context.get<List<PlayerSession>>("targets")
            for (target in targets) {
                target.inventory[slot] = source
                target.connection.sendInventoryAll()
            }
            context.success(
                Component.translatable(
                    "commands.item.entity.set.success.single",
                    Component.text(targets.firstOrNull()?.name.orEmpty()),
                    Component.text(source.id),
                ),
                broadcastToAdmins = true,
            )
            targets.size
        } else {
            writeIntoBlock(context, slot, source)
            1
        }
    }

    /** The stack this form reads from, whether that is a container or another player. */
    private fun readSource(context: CommandContext): StoredItemStack? {
        val slot = slotOf(context, "sourceSlot")
        if (context.has("source") && context.get<Any>("source") is BlockCoordinates) {
            return containerItems(context, context.get("source"))[slot]
        }
        val player = context.get<List<PlayerSession>>("source").single()
        return player.inventory[slot]
    }

    private fun slotOf(
        context: CommandContext,
        name: String,
    ): Int {
        val slotName = context.get<String>(name)
        return ItemSlots.resolve(slotName)
            ?: throw CommandSyntaxException(Component.translatable("slot.unknown", Component.text(slotName)))
    }

    private fun replaceInBlock(
        context: CommandContext,
        count: Int,
    ): Int {
        val parsed = context.get<ParsedItem>("item")
        writeIntoBlock(
            context,
            slotOf(context, "slot"),
            StoredItemStack(parsed.id.toString(), count, parsed.components),
        )
        return 1
    }

    /**
     * parity: a container's contents live in its block entity's `Items` list, one entry per filled
     * slot. A block with no block entity has no slots and says so.
     */
    private fun writeIntoBlock(
        context: CommandContext,
        slot: Int,
        stack: StoredItemStack,
    ) {
        val (x, y, z) = context.get<BlockCoordinates>("pos").resolve(context.source.position)
        val chunk =
            context.world().state.chunkIfLoaded(x shr 4, z shr 4)
                ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
        val existing =
            chunk.blockEntityAt(x and 15, y, z and 15)
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.item.target.not_a_container", Component.text("$x $y $z")),
                )
        val items = (existing.data["Items"] as? NbtList)?.filterIsInstance<NbtCompound>().orEmpty()
        val kept = items.filter { it.getInt("Slot") != slot }
        val written =
            NbtCompound().apply {
                putInt("Slot", slot)
                putString("id", stack.id)
                putInt("count", stack.count)
            }
        chunk.setBlockEntity(
            existing.withData(
                existing.data.also {
                    it["Items"] = NbtList(NbtType.COMPOUND, (kept + written).map { entry -> entry as NbtTag })
                },
            ),
        )
        context.success(
            Component.translatable(
                "commands.item.block.set.success",
                Component.text(x.toString()),
                Component.text(y.toString()),
                Component.text(z.toString()),
                Component.text(stack.id),
            ),
            broadcastToAdmins = true,
        )
    }

    private fun containerItems(
        context: CommandContext,
        pos: BlockCoordinates,
    ): Map<Int, StoredItemStack> {
        val (x, y, z) = pos.resolve(context.source.position)
        val chunk = context.world().state.chunkIfLoaded(x shr 4, z shr 4) ?: return emptyMap()
        val data = chunk.blockEntityAt(x and 15, y, z and 15)?.data ?: return emptyMap()
        val items = (data["Items"] as? NbtList)?.filterIsInstance<NbtCompound>().orEmpty()
        return items.associate { it.getInt("Slot") to StoredItemStack(it.getString("id"), it.getInt("count")) }
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
 * parity: `LootCommand`: four destinations and four sources, each pairing the same way.
 *
 * `mine` and `loot` roll a real table; `kill` rolls the entity's own; `fish` rolls the table it is
 * given, since fishing itself does not exist here. The tool argument is parsed and ignored: loot
 * conditions on it are not modelled, so it would change nothing.
 */
@OptIn(ExperimentalUuidApi::class)
public class LootCommand(
    private val lootTables: RealLootTables,
    private val worldSeed: () -> Long,
) : Command(name = "loot", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("give") {
            argument("players", PlayersArgument(single = false)) {
                sources {
                    context,
                    stacks,
                    ->
                    give(context, stacks)
                }
            }
        }
        root.literal("insert") {
            argument("targetPos", BlockPosArgument) { sources { context, stacks -> insert(context, stacks) } }
        }
        root.literal("spawn") {
            argument("targetPos", Vec3Argument) { sources { context, stacks -> spawn(context, stacks) } }
        }
        root.literal("replace") {
            literal("block") {
                argument("targetPos", BlockPosArgument) {
                    argument("slot", ItemSlotArgument) {
                        sources { context, stacks -> replaceInto(context, stacks, entities = false) }
                        argument("count", IntegerArgument(min = 0, max = MAX_COUNT)) {
                            sources { context, stacks -> replaceInto(context, stacks, entities = false) }
                        }
                    }
                }
            }
            literal("entity") {
                argument("entities", PlayersArgument(single = false)) {
                    argument("slot", ItemSlotArgument) {
                        sources { context, stacks -> replaceInto(context, stacks, entities = true) }
                        argument("count", IntegerArgument(min = 0, max = MAX_COUNT)) {
                            sources { context, stacks -> replaceInto(context, stacks, entities = true) }
                        }
                    }
                }
            }
        }
    }

    /** The four sources, declared once per destination the way vanilla pairs them. */
    private fun CommandNodeBuilder.sources(run: (CommandContext, List<StoredItemStack>) -> Int) {
        literal("fish") {
            argument("loot_table", LootTableArgument) {
                suggestsFromServer { context -> context.matchingIds(lootTables.ids()) }
                argument("pos", BlockPosArgument) {
                    executes { context -> run(context, rollTable(context)) }
                    for (hand in listOf("mainhand", "offhand")) {
                        literal(hand) { executes { context -> run(context, rollTable(context)) } }
                    }
                    argument("tool", ItemArgument) { executes { context -> run(context, rollTable(context)) } }
                }
            }
        }
        literal("kill") {
            argument("target", EntitiesArgument(single = true)) {
                executes { context -> run(context, rollEntity(context)) }
            }
        }
        literal("loot") {
            argument("loot_table", LootTableArgument) {
                suggestsFromServer { context -> context.matchingIds(lootTables.ids()) }
                executes { context -> run(context, rollTable(context)) }
            }
        }
        literal("mine") {
            argument("pos", BlockPosArgument) {
                executes { context -> run(context, rollBlock(context)) }
                for (hand in listOf("mainhand", "offhand")) {
                    literal(hand) { executes { context -> run(context, rollBlock(context)) } }
                }
                argument("tool", ItemArgument) { executes { context -> run(context, rollBlock(context)) } }
            }
        }
    }

    private fun rollTable(context: CommandContext): List<StoredItemStack> {
        val id = context.get<Identifier>("loot_table")
        val table =
            lootTables.byId(id)
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.drop.no_loot_table", Component.text(id.toString())),
                )
        return table.generate(LegacyRandom(worldSeed() xor id.toString().hashCode().toLong())).map(::stackOf)
    }

    private fun rollEntity(context: CommandContext): List<StoredItemStack> {
        val target = context.get<List<CommandTarget>>("target").single()
        val table =
            lootTables.forEntity(target.type)
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.drop.no_loot_table.entity", Component.text(target.name)),
                )
        return table.generate(LegacyRandom(worldSeed() xor target.entityId.toLong())).map(::stackOf)
    }

    private fun rollBlock(context: CommandContext): List<StoredItemStack> {
        val world = context.world()
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
        return table.generate(LegacyRandom(worldSeed() xor ((x.toLong() shl 32) or z.toLong()))).map(::stackOf)
    }

    private fun stackOf(stack: ItemStack): StoredItemStack = StoredItemStack(stack.item.toString(), stack.count)

    private fun give(
        context: CommandContext,
        stacks: List<StoredItemStack>,
    ): Int {
        val players = context.get<List<PlayerSession>>("players")
        for (player in players) {
            for (stack in stacks) player.inventory.add(stack)
            player.connection.sendInventoryAll()
        }
        report(context, stacks)
        return stacks.size * players.size
    }

    private fun spawn(
        context: CommandContext,
        stacks: List<StoredItemStack>,
    ): Int {
        val world = context.world()
        val (x, y, z) = context.get<Coordinates>("targetPos").resolve(context.source.position)
        for (stack in stacks) {
            world.state.spawnItem(
                WorldState.DroppedItem(
                    entityId = context.environment.players.allocateEntityId(),
                    uuid = Uuid.random(),
                    item = Identifier.parse(stack.id),
                    count = stack.count,
                    x = x,
                    y = y,
                    z = z,
                ),
            )
        }
        report(context, stacks)
        return stacks.size
    }

    private fun insert(
        context: CommandContext,
        stacks: List<StoredItemStack>,
    ): Int {
        val (x, y, z) = context.get<BlockCoordinates>("targetPos").resolve(context.source.position)
        val chunk =
            context.world().state.chunkIfLoaded(x shr 4, z shr 4)
                ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
        val existing =
            chunk.blockEntityAt(x and 15, y, z and 15)
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.item.target.not_a_container", Component.text("$x $y $z")),
                )
        val items = (existing.data["Items"] as? NbtList)?.filterIsInstance<NbtCompound>().orEmpty().toMutableList()
        var slot = 0
        for (stack in stacks) {
            while (items.any { it.getInt("Slot") == slot }) slot++
            items +=
                NbtCompound().apply {
                    putInt("Slot", slot)
                    putString("id", stack.id)
                    putInt("count", stack.count)
                }
        }
        chunk.setBlockEntity(
            existing.withData(
                existing.data.also {
                    it["Items"] =
                        NbtList(NbtType.COMPOUND, items.map { entry -> entry as NbtTag })
                },
            ),
        )
        report(context, stacks)
        return stacks.size
    }

    private fun replaceInto(
        context: CommandContext,
        stacks: List<StoredItemStack>,
        entities: Boolean,
    ): Int {
        val slotName = context.get<String>("slot")
        val slot =
            ItemSlots.resolve(slotName)
                ?: throw CommandSyntaxException(Component.translatable("slot.unknown", Component.text(slotName)))
        val first = stacks.firstOrNull() ?: return 0
        if (entities) {
            val targets = context.get<List<PlayerSession>>("entities")
            for (target in targets) {
                target.inventory[slot] = first
                target.connection.sendInventoryAll()
            }
            report(context, stacks)
            return targets.size
        }
        val (x, y, z) = context.get<BlockCoordinates>("targetPos").resolve(context.source.position)
        val chunk =
            context.world().state.chunkIfLoaded(x shr 4, z shr 4)
                ?: throw CommandSyntaxException(Component.translatable("argument.pos.unloaded"))
        val existing =
            chunk.blockEntityAt(x and 15, y, z and 15)
                ?: throw CommandSyntaxException(
                    Component.translatable("commands.item.target.not_a_container", Component.text("$x $y $z")),
                )
        val kept =
            (existing.data["Items"] as? NbtList)
                ?.filterIsInstance<NbtCompound>()
                .orEmpty()
                .filter { it.getInt("Slot") != slot }
        val written =
            NbtCompound().apply {
                putInt("Slot", slot)
                putString("id", first.id)
                putInt("count", first.count)
            }
        chunk.setBlockEntity(
            existing.withData(
                existing.data.also {
                    it["Items"] = NbtList(NbtType.COMPOUND, (kept + written).map { entry -> entry as NbtTag })
                },
            ),
        )
        report(context, stacks)
        return 1
    }

    private fun report(
        context: CommandContext,
        stacks: List<StoredItemStack>,
    ) {
        if (stacks.isEmpty()) {
            context.success(Component.translatable("commands.drop.no_loot_table.block", Component.text("")))
            return
        }
        context.success(
            Component.translatable(
                "commands.drop.success.multiple",
                Component.text(stacks.sumOf { it.count }.toString()),
            ),
            broadcastToAdmins = true,
        )
    }

    private companion object {
        const val MAX_COUNT = 99
    }
}
