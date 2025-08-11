package org.basaltmc.server.runtime

import org.basaltmc.server.plugins.PluginEventTypes
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.BlockSupport
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Fuels
import org.basaltmc.server.registry.ItemBlocks
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.entity.EntityPickResults
import org.basaltmc.server.runtime.item.ItemComponentCodecs
import org.basaltmc.server.runtime.player.Menu
import org.basaltmc.server.runtime.player.MenuSlot
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.BlockEffect
import org.basaltmc.server.runtime.world.BlockInventory
import org.basaltmc.server.runtime.world.ContainerOpeners
import org.basaltmc.server.runtime.world.Containers
import org.basaltmc.server.runtime.world.DoubleChests
import org.basaltmc.server.runtime.world.Furnaces
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.Lecterns
import org.basaltmc.server.runtime.world.Shulkers
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.server.storage.StoredPlayer
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.crafting.CraftingInput
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.BlockEventPacket
import org.basaltmc.wire.protocol.play.BlockPos
import org.basaltmc.wire.protocol.play.ContainerButtonClickPacket
import org.basaltmc.wire.protocol.play.ContainerClosePacket
import org.basaltmc.wire.protocol.play.ContainerSetContentPacket
import org.basaltmc.wire.protocol.play.ContainerSetDataPacket
import org.basaltmc.wire.protocol.play.ContainerSetSlotPacket
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.protocol.play.OpenScreenPacket
import org.basaltmc.wire.protocol.play.ServerboundContainerClickPacket
import org.basaltmc.wire.protocol.play.ServerboundPickItemFromBlockPacket
import org.basaltmc.wire.protocol.play.ServerboundPickItemFromEntityPacket
import org.basaltmc.wire.protocol.play.ServerboundSetCreativeModeSlotPacket
import org.basaltmc.wire.protocol.play.SetHeldSlotPacket
import org.basaltmc.wire.protocol.play.SoundPacket
import org.basaltmc.wire.protocol.play.SoundSource
import org.basaltmc.wire.protocol.play.WireItemStack
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.ComponentNbt
import org.basaltmc.wire.text.MiniMessage
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.random.Random
import org.basaltmc.server.world.loot.ItemStack as LootItemStack

/**
 * Container, menu, inventory and crafting state: every window this player can have open, and what
 * a click, a shift-click or a drag does to it. Kept as one class: [readBlockSlot],
 * [writeBlockSlot], [canPlaceInto], [accepts], [hasBlockSlots] and [closeOpenMenu] all switch on
 * the same mutually exclusive "which window is open" fields, and splitting that check across
 * files would either duplicate it or reintroduce a shared state holder anyway.
 */
internal class ContainerMenuController(
    private val owner: ClientSession,
) {
    // Vanilla's optimistic-locking counter for container changes: bumped on every window write so
    // the client can tell a click made against the current contents from a stale one.
    private var containerStateId = 0

    /** The window this player has open. Their own inventory unless they opened a block's. */
    internal var openMenu: Menu = Menu.playerInventory()

    /** The last container id handed out, so a freshly opened window never reuses the previous one. */
    internal var nextContainerId = ClientSession.FIRST_BLOCK_CONTAINER_ID

    /** The crafting grid of [openMenu], indexed row-major. Sized for the largest grid any window has. */
    private val craftingGrid = arrayOfNulls<StoredItemStack>(ClientSession.MAX_GRID_SIZE)

    /** What [craftingGrid] currently makes, recomputed whenever it changes. */
    private var craftingResult: StoredItemStack? = null

    /** The cooker whose window is open, if the open window belongs to one. */
    internal var openBlock: BlockPos? = null

    /**
     * Whether the open screen is this player's own ender chest rather than a block's slots.
     *
     * parity: `PlayerEnderChestContainer` belongs to the player, so the twenty-seven slots come off
     * the session and go back to `players.db`, not to the chest that was clicked.
     */
    private var openEnderChest: Boolean = false

    /**
     * The window a plugin drew, if one is open: its contents and where a click goes.
     *
     * Not a container. Nothing in it can be taken, because a menu is a row of buttons; a plugin that
     * wants somewhere to put things wants a chest, which the world already has.
     */
    private var pluginMenu: Array<StoredItemStack?>? = null
    private var pluginMenuClick: ((Int) -> Unit)? = null

    /**
     * The other half of an open double chest, whose slots follow the first half's.
     *
     * parity: `ChestBlock.DOUBLE_INVENTORY_COMBINER` over a `CompoundContainer`: two block entities,
     * one window, and which of them leads is decided by the state rather than by which was clicked.
     */
    internal var openBlockSecond: BlockPos? = null

    /**
     * Where the opener count was taken, which is the block that was clicked.
     *
     * Not the same as [openBlock] since a double chest opens as its *first* half whichever end was
     * clicked: counting the opening on one block and the closing on another left the lid up for
     * good and a trapped chest powered for good.
     */
    private var openedAt: BlockPos? = null

    /** Whether the open screen is a stonecutter, whose two slots are the menu's own and not a block's. */
    private var openStonecutter: Boolean = false

    /** parity: `StonecutterMenu.input`/`resultSlot`, which live in the menu rather than in the block. */
    private var stonecutterInput: StoredItemStack? = null
    private var stonecutterResult: StoredItemStack? = null

    /** Whether the open screen is a smithing table, whose four slots are the menu's own. */
    private var openSmithing: Boolean = false

    /** parity: `SmithingMenu`'s three inputs and its result, none of which belong to the block. */
    private val smithingSlots = arrayOfNulls<StoredItemStack>(4)

    /** What the open cooker last showed, so a bar is only re-sent when it moves. */
    private var openBlockData = IntArray(4) { -1 }

    /** And the same for its three slots, which the fire changes without anybody clicking. */
    private val shownBlockSlots = arrayOfNulls<StoredItemStack>(3)

    /** How many slots the open block has, which is what tells a chest from a cooker. */
    internal var openBlockSize = ClientSession.FURNACE_SLOTS

    /**
     * What this player is holding on the cursor inside their open window, kept server-side rather
     * than taken from the client's own claim: trusting theirs is how a stack gets duplicated.
     */
    private var carriedItem: StoredItemStack? = null

    // The click-and-drag gesture in progress, if any. See [handleQuickCraft].
    private var quickCraftActive = false
    private var quickCraftType = 0
    private val quickCraftSlots = LinkedHashSet<Int>()

    /** The lectern this client has open, and the page it is showing. */
    internal var openLecternAt: BlockPos? = null
    private var lecternPage: Int = 0

    /**
     * Opens a crafting table's 3x3 window.
     *
     * The container id has to change every time a window is opened: the client tags every click
     * with it, and reusing one leaves clicks meant for a window that has already closed being
     * applied to the next one.
     */
    internal fun openCraftingTable(session: PlayerSession) {
        val opening = owner.playerEvent(PluginEventTypes.INVENTORY_OPEN) { event -> event.set(3, "crafting") }
        if (opening?.cancelled == true) {
            opening.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        returnCraftingGrid(session)
        nextContainerId =
            if (nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                nextContainerId + 1
            }
        openMenu = Menu.craftingTable(nextContainerId)
        recomputeCraftingResult()
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.CRAFTING_MENU,
                title = Component.translatable("container.crafting"),
            ).write(it)
        }
        sendInventory()
    }

    /**
     * Opens a furnace, a blast furnace or a smoker.
     *
     * parity: `AbstractFurnaceBlock.use`, which opens the block's own menu. The three slots are the
     * block's, so nothing is copied into the session: a click reads and writes the block entity.
     */
    internal fun openFurnace(
        session: PlayerSession,
        pos: BlockPos,
        cooker: String,
    ) {
        val opening =
            owner.playerEvent(PluginEventTypes.INVENTORY_OPEN) { event ->
                event.set(3, cooker)
                event.set(4, pos.x)
                event.set(5, pos.y)
                event.set(6, pos.z)
            }
        if (opening?.cancelled == true) {
            opening.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        returnCraftingGrid(session)
        nextContainerId =
            if (nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                nextContainerId + 1
            }
        openMenu = Menu.furnace(nextContainerId)
        openBlock = pos
        openBlockSize = ClientSession.FURNACE_SLOTS
        openBlockData = IntArray(4) { -1 }
        val menuType =
            when (cooker) {
                "blast_furnace" -> OpenScreenPacket.BLAST_FURNACE_MENU
                "smoker" -> OpenScreenPacket.SMOKER_MENU
                else -> OpenScreenPacket.FURNACE_MENU
            }
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = menuType,
                title = Component.translatable("container.$cooker"),
            ).write(it)
        }
        sendInventory()
        sendFurnaceBars()
    }

    /**
     * parity: `EnderChestBlock.useWithoutItem`: the same twenty-seven slots wherever the chest is,
     * and nothing at all when something solid sits on top of it, exactly like a chest.
     */
    internal fun openEnderChest(
        session: PlayerSession,
        pos: BlockPos,
    ) {
        val world = owner.currentWorld ?: return
        val above = world.state.getBlockState(pos.x, pos.y + 1, pos.z) ?: return
        if (BlockSupport.conductsRedstone(above)) return
        returnCraftingGrid(session)
        nextContainerId =
            if (nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                nextContainerId + 1
            }
        openMenu = Menu.chest(nextContainerId)
        openBlock = null
        openBlockSecond = null
        openEnderChest = true
        openBlockSize = StoredPlayer.ENDER_CHEST_SLOTS
        openBlockData = IntArray(4) { -1 }
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.CHEST_MENU,
                title = Component.translatable("container.enderchest"),
            ).write(it)
        }
        sendInventory()
        owner.broadcastEffects(
            pos,
            world.state.getBlockState(pos.x, pos.y, pos.z) ?: 0,
            listOf(BlockEffect.Sound("block.ender_chest.open", SoundSource.BLOCKS, 0.5f, 1f)),
        )
    }

    /**
     * What a container's window is called.
     *
     * parity: the name comes from the *block entity*, not the block, so all ten chests are
     * `container.chest` and every shulker box is `container.shulkerBox`. Naming them after the block
     * left a copper chest showing `container.waxed_copper_chest` in raw text.
     */
    private fun containerTitleKey(
        block: String,
        paired: Boolean,
    ): String =
        when {
            Shulkers.isShulkerBox(block) -> "container.shulkerBox"
            paired -> "container.chestDouble"
            block in Containers.CHESTS -> "container.chest"
            else -> "container.$block"
        }

    /**
     * Opens a chest, a trapped chest, a barrel or a shulker box.
     *
     * parity: `ChestBlock.use`, which opens the block's own container. Double chests are not paired
     * yet, so each half opens as its own three rows.
     */
    internal fun openChest(
        session: PlayerSession,
        pos: BlockPos,
        block: String,
    ) {
        returnCraftingGrid(session)
        nextContainerId =
            if (nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                nextContainerId + 1
            }
        // parity: `ChestBlock.getMenuProvider`, which hands both halves over as one container.
        val halves = DoubleChests.halves(owner.currentWorld, pos.x, pos.y, pos.z)
        openEnderChest = false
        openBlockSecond = halves?.second?.let { BlockPos(it[0], it[1], it[2]) }
        openBlock = halves?.first?.let { BlockPos(it[0], it[1], it[2]) } ?: pos
        val paired = openBlockSecond != null
        openMenu = if (paired) Menu.doubleChest(nextContainerId) else Menu.chest(nextContainerId)
        openBlockSize = if (paired) Menu.DOUBLE_CHEST_SIZE else Menu.CHEST_SIZE
        openBlockData = IntArray(4) { -1 }
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = if (paired) OpenScreenPacket.DOUBLE_CHEST_MENU else OpenScreenPacket.CHEST_MENU,
                title = Component.translatable(containerTitleKey(block, paired)),
            ).write(it)
        }
        sendInventory()
        openedAt = pos
        announceOpeners(pos, block, ContainerOpeners.open(session.worldName, pos.x, pos.y, pos.z))
        // parity: `ChestBlock`'s lid is per block entity, so both halves of a double chest open.
        openBlockSecond?.let { other ->
            val at = if (openBlock == pos) other else checkNotNull(openBlock)
            announceOpeners(at, block, ContainerOpeners.open(session.worldName, at.x, at.y, at.z))
        }
    }

    /**
     * What everyone else sees when somebody opens or closes a container: the lid, and the sound.
     *
     * parity: `ContainerOpenersCounter`, which only calls `onOpen` for the first person in and
     * `onClose` for the last one out, plus `ChestBlockEntity.signalOpenCount`'s block event on every
     * change. A barrel has no lid to animate: it carries `open` in its own state instead.
     */
    private fun announceOpeners(
        pos: BlockPos,
        block: String,
        openers: Int,
    ) {
        val world = owner.currentWorld ?: return
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return
        if (block == "barrel") {
            val opened =
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("barrel"),
                    mapOf("open" to (openers > 0).toString()),
                    stateId,
                )
            if (opened != null && opened != stateId && world.state.setBlockState(pos.x, pos.y, pos.z, opened)) {
                owner.broadcastBlockUpdate(pos, opened)
            }
        } else {
            val blockId = BlockSupport.blockRegistryIdOf(stateId)
            for (player in owner.playersInSameWorld()) {
                player.connection.sendPlayPacket(BlockEventPacket.ID) {
                    BlockEventPacket(pos, b0 = 1, b1 = openers, blockId = blockId).write(it)
                }
            }
        }
        // Only the first in and the last out make a sound, which is why a busy chest is not a rattle.
        val family =
            when {
                block == "barrel" -> "barrel"
                Shulkers.isShulkerBox(block) -> "shulker_box"
                else -> "chest"
            }
        val sound =
            when (openers) {
                1 -> "block.$family.open"
                0 -> "block.$family.close"
                else -> return
            }
        for (player in owner.playersInSameWorld()) {
            player.connection.sendPlayPacket(SoundPacket.ID) {
                SoundPacket(
                    sound = sound,
                    source = SoundSource.BLOCKS,
                    x = pos.x + 0.5,
                    y = pos.y + 0.5,
                    z = pos.z + 0.5,
                    volume = 0.5f,
                    // parity: `ChestBlockEntity.playSound`, a tenth of a tone of randomness.
                    pitch = Random.nextFloat() * 0.1f + 0.9f,
                    seed = Random.nextLong(),
                ).write(it)
            }
        }
    }

    /**
     * Opens a window a plugin drew, [rows] rows of nine with [items] in it.
     *
     * Built on the same [Menu] a chest uses, so clicks arrive through the same path and the player's
     * own inventory keeps its usual place at the bottom. What is different is behind the slots:
     * there is no block, the contents are this array, and no click ever moves any of it.
     */
    internal fun openPluginMenu(
        title: String,
        rows: Int,
        items: Map<Int, org.basaltmc.server.plugins.PluginItem>,
        onClick: (Int) -> Unit,
    ) {
        val session = owner.playerSession ?: return
        returnCraftingGrid(session)
        nextContainerId =
            if (nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                nextContainerId + 1
            }
        openMenu = Menu.rows(nextContainerId, rows)
        openBlock = null
        openBlockSecond = null
        openEnderChest = false
        openStonecutter = false
        openSmithing = false
        openBlockSize = rows * Menu.COLUMNS
        openBlockData = IntArray(4) { -1 }
        pluginMenu = arrayOfNulls(rows * Menu.COLUMNS)
        pluginMenuClick = onClick
        fillPluginMenu(items)
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                // `minecraft:generic_9x1` through `9x6` are consecutive from zero.
                menuType = rows - 1,
                title = MiniMessage.parse(title),
            ).write(it)
        }
        sendInventory()
    }

    /**
     * Shuts whatever window is open, telling the client too.
     *
     * The client is told because this closing did not start there: a plugin decided, or was
     * unloaded, and a window nobody takes down stays on the screen doing nothing.
     */
    internal fun closeOpenMenu() {
        val session = owner.playerSession ?: return
        if (openMenu.containerId == 0) return
        owner.sendPlayPacket(ContainerClosePacket.CLIENTBOUND_ID) {
            ContainerClosePacket(openMenu.containerId).write(it)
        }
        closeOpenMenu(session)
    }

    /** Puts new contents in the open menu and redraws it. */
    internal fun refreshPluginMenu(items: Map<Int, org.basaltmc.server.plugins.PluginItem>) {
        if (pluginMenu == null) return
        fillPluginMenu(items)
        sendInventory()
    }

    private fun fillPluginMenu(items: Map<Int, org.basaltmc.server.plugins.PluginItem>) {
        val slots = pluginMenu ?: return
        slots.fill(null)
        for ((slot, item) in items) {
            if (slot !in slots.indices) continue
            slots[slot] = pluginMenuStack(item)
        }
    }

    private fun pluginMenuStack(item: org.basaltmc.server.plugins.PluginItem): StoredItemStack {
        val components = NbtCompound()
        // The name is MiniMessage on the plugin's side and a component on the wire, so it is parsed
        // here rather than stored as the tags a client would show literally.
        item.name?.let { components[ItemComponentCodecs.CUSTOM_NAME] = ComponentNbt.serialize(MiniMessage.parse(it)) }
        return StoredItemStack(item.id, item.count.coerceAtLeast(1), components)
    }

    /**
     * parity: `SmithingTableBlock.useWithoutItem` over `SmithingMenu`: a template, a base and an
     * addition, and whatever the three of them make. Like the stonecutter, the slots are the
     * screen's own, so what is left in them comes back when it closes.
     */
    internal fun openSmithingMenu(session: PlayerSession) {
        returnCraftingGrid(session)
        nextContainerId =
            if (nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                nextContainerId + 1
            }
        openMenu = Menu.smithing(nextContainerId)
        openBlock = null
        openBlockSecond = null
        openEnderChest = false
        openStonecutter = false
        openSmithing = true
        smithingSlots.fill(null)
        openBlockSize = smithingSlots.size
        openBlockData = IntArray(4) { -1 }
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.SMITHING_MENU,
                title = Component.translatable("container.upgrade"),
            ).write(it)
        }
        sendInventory()
    }

    /** parity: `SmithingMenu.createResult`, run whenever any of the three inputs changes. */
    private fun recomputeSmithingResult() {
        val recipes = owner.recipes
        val template = smithingSlots[Menu.SMITHING_TEMPLATE]?.let(::craftingStackOf)
        val base = smithingSlots[Menu.SMITHING_BASE]?.let(::craftingStackOf)
        val addition = smithingSlots[Menu.SMITHING_ADDITION]?.let(::craftingStackOf)
        val recipe = recipes?.smithingFor(template, base, addition)
        smithingSlots[Menu.SMITHING_RESULT] =
            if (recipe == null || base == null || addition == null) {
                null
            } else {
                recipe.assemble(base, addition)?.let { StoredItemStack(it.item.toString(), it.count, it.components) }
            }
    }

    /** parity: `SmithingMenu.onTake`, which eats one of each of the three inputs. */
    private fun takeSmithingResult() {
        for (slot in listOf(Menu.SMITHING_TEMPLATE, Menu.SMITHING_BASE, Menu.SMITHING_ADDITION)) {
            val stack = smithingSlots[slot] ?: continue
            smithingSlots[slot] = if (stack.count <= 1) null else stack.copy(count = stack.count - 1)
        }
        recomputeSmithingResult()
    }

    /** A stored stack as the crafting side sees it, components and all. */
    private fun craftingStackOf(stack: StoredItemStack): org.basaltmc.server.world.loot.ItemStack =
        org.basaltmc.server.world.loot
            .ItemStack(Identifier.parse(stack.id), stack.count, stack.components)

    /**
     * parity: `StonecutterBlock.useWithoutItem` over `StonecutterMenu`.
     *
     * Its two slots are the menu's own, not a block's: whatever is left in the input when the screen
     * closes comes back to the player, exactly as vanilla's `removed` does.
     */
    internal fun openStonecutterMenu(session: PlayerSession) {
        returnCraftingGrid(session)
        nextContainerId =
            if (nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                nextContainerId + 1
            }
        openMenu = Menu.stonecutter(nextContainerId)
        openBlock = null
        openBlockSecond = null
        openEnderChest = false
        openStonecutter = true
        stonecutterInput = null
        stonecutterResult = null
        openBlockSize = 2
        openBlockData = IntArray(4) { -1 }
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.STONECUTTER_MENU,
                title = Component.translatable("container.stonecutter"),
            ).write(it)
        }
        sendInventory()
    }

    /** parity: `StonecutterMenu.clickMenuButton` into `setupResultSlot`. */
    private fun selectStonecutterRecipe(
        session: PlayerSession,
        button: Int,
    ) {
        val input = stonecutterInput
        val recipes = owner.recipes
        if (input == null || recipes == null) {
            stonecutterResult = null
            sendInventory()
            return
        }
        val options =
            recipes.stonecuttingFor(
                org.basaltmc.server.world.loot
                    .ItemStack(Identifier.parse(input.id), input.count),
            )
        val chosen = options.getOrNull(button)
        stonecutterResult = chosen?.let { StoredItemStack(it.result.item.toString(), it.result.count) }
        sendInventory()
    }

    /**
     * parity: `StonecutterMenu.ResultSlot.onTake`: taking the result eats one of the input and
     * leaves the same button selected, so a stack can be cut one at a time without clicking again.
     */
    private fun takeStonecutterResult(session: PlayerSession) {
        val input = stonecutterInput ?: return
        stonecutterInput = if (input.count <= 1) null else input.copy(count = input.count - 1)
        if (stonecutterInput == null) {
            stonecutterResult = null
        }
    }

    /**
     * A button inside an open window: the lectern's pages, or the stonecutter's list.
     *
     * parity: `LecternMenu.clickMenuButton` (one back, two forward, three to take the book, and a
     * hundred plus the page to jump to it) and `StonecutterMenu.clickMenuButton`, whose button is an
     * index into the recipes that fit the input.
     */
    internal fun clickContainerButton(button: Int) {
        val session = owner.playerSession ?: return
        val world = owner.currentWorld ?: return
        // parity: `StonecutterMenu.clickMenuButton`, whose button is an index into the recipes that
        // fit whatever is in the input slot, in the order the client was sent them.
        if (openStonecutter) {
            selectStonecutterRecipe(session, button)
            return
        }
        val pos = openLecternAt ?: return
        val page = lecternPage
        val wanted =
            when {
                button >= ContainerButtonClickPacket.LECTERN_JUMP_TO_PAGE ->
                    button - ContainerButtonClickPacket.LECTERN_JUMP_TO_PAGE
                button == ContainerButtonClickPacket.LECTERN_PREVIOUS_PAGE -> page - 1
                button == ContainerButtonClickPacket.LECTERN_NEXT_PAGE -> page + 1
                button == ContainerButtonClickPacket.LECTERN_TAKE_BOOK -> {
                    val taken = Lecterns.takeBook(world, pos.x, pos.y, pos.z) ?: return
                    val (change, book) = taken
                    owner.broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
                    val left = session.inventory.add(StoredItemStack(book.toString(), 1))
                    if (left > 0) owner.spawnDroppedItem(book, left, session.x, session.y, session.z)
                    sendInventory()
                    owner.notifyNeighbours(pos)
                    openLecternAt = null
                    return
                }
                else -> return
            }
        if (wanted < 0) return
        lecternPage = wanted
        val change = Lecterns.turnPage(world, pos.x, pos.y, pos.z, wanted) ?: return
        owner.broadcastBlockUpdate(BlockPos(change.x, change.y, change.z), change.stateId)
        owner.notifyNeighbours(pos)
    }

    /**
     * Opens a crafter.
     *
     * parity: `CrafterBlock.useWithoutItem`. Its nine slots are the block's own, like a chest's, and
     * nothing is taken out by hand: the block pushes what it makes out of its front.
     */
    internal fun openCrafter(
        session: PlayerSession,
        pos: BlockPos,
    ) {
        returnCraftingGrid(session)
        nextContainerId =
            if (nextContainerId >= ClientSession.MAX_CONTAINER_ID) {
                ClientSession.FIRST_BLOCK_CONTAINER_ID
            } else {
                nextContainerId + 1
            }
        openMenu = Menu.crafter(nextContainerId)
        openBlock = pos
        openBlockSize = Menu.CRAFTER_SIZE
        openBlockData = IntArray(4) { -1 }
        owner.sendCompressed(OpenScreenPacket.ID) {
            OpenScreenPacket(
                containerId = openMenu.containerId,
                menuType = OpenScreenPacket.CRAFTER_MENU,
                title = Component.translatable("container.crafter"),
            ).write(it)
        }
        sendInventory()
    }

    /** The block's own slots, read straight off its block entity. */
    private fun readBlockSlot(index: Int): StoredItemStack? {
        if (openSmithing) return smithingSlots.getOrNull(index)
        if (openStonecutter) {
            return if (index == Menu.STONECUTTER_INPUT) stonecutterInput else stonecutterResult
        }
        pluginMenu?.let { return it.getOrNull(index) }
        if (openEnderChest) return owner.playerSession?.enderChest?.getOrNull(index)
        val (position, local) = halfFor(index) ?: return null
        val entity = blockEntityAt(position) ?: return null
        val stack = BlockInventory.read(entity.data, halfSize()).getOrNull(local) ?: return null
        return StoredItemStack(stack.item.toString(), stack.count)
    }

    /**
     * Which block a window slot belongs to, and its index within that block.
     *
     * A double chest is two containers of 27 behind one window of 54, so everything past the first
     * half lands in the second block. Anything else is its own block and its own numbering.
     */
    private fun halfFor(index: Int): Pair<BlockPos, Int>? {
        val second = openBlockSecond
        if (second != null && index >= Menu.CHEST_SIZE) return second to (index - Menu.CHEST_SIZE)
        return (openBlock ?: return null) to index
    }

    /** How many slots the block behind a window slot has of its own. */
    private fun halfSize(): Int = if (openBlockSecond != null) Menu.CHEST_SIZE else openBlockSize

    private fun blockEntityAt(position: BlockPos): org.basaltmc.server.world.ChunkBlockEntity? {
        val world = owner.currentWorld ?: return null
        val chunk = world.state.chunkIfLoaded(position.x shr 4, position.z shr 4) ?: return null
        return chunk.blockEntityAt(position.x and 15, position.y, position.z and 15)
    }

    private fun openBlockEntity(): org.basaltmc.server.world.ChunkBlockEntity? = blockEntityAt(openBlock ?: return null)

    private fun writeBlockSlot(
        index: Int,
        stack: StoredItemStack?,
    ) {
        if (openSmithing) {
            if (index !in smithingSlots.indices) return
            if (index == Menu.SMITHING_RESULT) {
                // Taking the result is the only way this slot is written, and it costs the inputs.
                if (stack == null) takeSmithingResult()
                return
            }
            smithingSlots[index] = stack
            recomputeSmithingResult()
            return
        }
        if (openStonecutter) {
            val session = owner.playerSession ?: return
            if (index == Menu.STONECUTTER_INPUT) {
                stonecutterInput = stack
                // parity: `slotsChanged`, which clears the choice whenever the input changes.
                stonecutterResult = null
            } else if (stack == null) {
                takeStonecutterResult(session)
            }
            return
        }
        if (openEnderChest) {
            val slots = owner.playerSession?.enderChest ?: return
            if (index in slots.indices) slots[index] = stack
            return
        }
        val (position, local) = halfFor(index) ?: return
        val world = owner.currentWorld ?: return
        val chunk = world.state.chunkIfLoaded(position.x shr 4, position.z shr 4) ?: return
        // A block that never got its block entity (one from worldgen, or one written by a command)
        // used to swallow whatever was put into it: the cursor was already empty and the write had
        // nowhere to land. It gets one here instead.
        val entity =
            chunk.blockEntityAt(position.x and 15, position.y, position.z and 15)
                ?: BlockStateDefinitions
                    .blockOf(world.state.getBlockState(position.x, position.y, position.z) ?: return)
                    ?.let {
                        ChunkBlockEntity(position.x and 15, position.y, position.z and 15, it, NbtCompound())
                            .also { entity -> chunk.setBlockEntity(entity) }
                    } ?: return
        // Every slot of the block, not the three a cooker has: writing back fewer than it holds is
        // what would empty a chest the moment somebody moved its first item.
        val slots = BlockInventory.read(entity.data, halfSize())
        if (local !in slots.indices) {
            owner.logger.warn { "dropped a write to slot $local of a container that has ${slots.size}" }
            return
        }
        slots[local] =
            stack?.let {
                org.basaltmc.server.world.loot
                    .ItemStack(Identifier.parse(it.id), it.count)
            }
        BlockInventory.write(entity.data, slots)
        // parity: `AbstractFurnaceBlockEntity.setItem`, which recomputes the cook time when the
        // thing being cooked changes: a new item does not inherit the last one's progress.
        val cooking = BlockStateDefinitions.blockOf(world.state.getBlockState(position.x, position.y, position.z) ?: 0)
        if (local == Furnaces.INPUT_SLOT && cooking?.path in Furnaces.COOKERS) {
            val state = Furnaces.read(entity.data)
            state.cookingTimer = 0
            state.cookingTotalTime = Furnaces.cookTimeFor(state) { source -> cookingFor(position, source) }
            Furnaces.write(state, entity.data)
        }
        chunk.markDirty()
    }

    private fun furnaceState(): Furnaces.State? {
        val position = openBlock ?: return null
        val world = owner.currentWorld ?: return null
        val chunk = world.state.chunkIfLoaded(position.x shr 4, position.z shr 4) ?: return null
        val entity = chunk.blockEntityAt(position.x and 15, position.y, position.z and 15) ?: return null
        return Furnaces.read(entity.data)
    }

    private fun cookingFor(
        position: BlockPos,
        stack: org.basaltmc.server.world.loot.ItemStack,
    ): org.basaltmc.server.world.crafting.Recipe.Cooking? {
        val world = owner.currentWorld ?: return null
        val block = BlockStateDefinitions.blockOf(world.state.getBlockState(position.x, position.y, position.z) ?: 0)
        val cooker = Furnaces.COOKERS[block?.path] ?: return null
        return owner.recipes?.cookingFor(cooker, stack)
    }

    /** Keeps an open cooker's window in step: its bars, and its slots as the fire eats them. */
    internal fun syncOpenContainer() {
        sendFurnaceBars()
        val session = owner.playerSession ?: return
        val state = furnaceState() ?: return
        for (index in state.items.indices) {
            val menuSlot = openMenu.menuSlotOf(MenuSlot.Block(index)) ?: continue
            val shown = shownBlockSlots.getOrNull(index)
            val now = state.items[index]?.let { StoredItemStack(it.item.toString(), it.count) }
            if (shown == now) continue
            if (index < shownBlockSlots.size) shownBlockSlots[index] = now
            owner.sendPlayPacket(ContainerSetSlotPacket.ID) {
                ContainerSetSlotPacket(
                    containerId = openMenu.containerId,
                    stateId = ++containerStateId,
                    slot = menuSlot,
                    item = session.inventory.toWire(now),
                ).write(it)
            }
        }
    }

    /**
     * The two bars of an open cooker, sent only when a number moves.
     *
     * parity: `AbstractContainerMenu.broadcastChanges`, which does the same for its data slots.
     */
    public fun sendFurnaceBars() {
        val state = furnaceState() ?: return
        val values =
            intArrayOf(state.litTimeRemaining, state.litTotalTime, state.cookingTimer, state.cookingTotalTime)
        for (index in values.indices) {
            if (values[index] == openBlockData[index]) continue
            openBlockData[index] = values[index]
            val value = values[index]
            owner.sendPlayPacket(ContainerSetDataPacket.ID) {
                ContainerSetDataPacket(openMenu.containerId, index, value).write(it)
            }
        }
    }

    /** Back to the player's own window, giving them back whatever was on the grid or the cursor. */
    internal fun closeOpenMenu(session: PlayerSession) {
        if (openMenu.containerId == 0) {
            // Their own inventory closing still has a 2x2 grid to empty.
            returnCraftingGrid(session)
            sendInventory()
            return
        }
        returnCraftingGrid(session)
        val closing = openedAt
        val closingSecond = openBlockSecond?.let { other -> if (openBlock == closing) other else openBlock }
        val closingBlock = closing?.let { openContainerBlock(it) }
        openMenu = Menu.playerInventory()
        openBlock = null
        openBlockSecond = null
        openEnderChest = false
        pluginMenu = null
        pluginMenuClick = null
        // parity: `SmithingMenu.removed`, which hands the three inputs back.
        if (openSmithing) {
            for (slot in listOf(Menu.SMITHING_TEMPLATE, Menu.SMITHING_BASE, Menu.SMITHING_ADDITION)) {
                smithingSlots[slot]?.let { leftover ->
                    val left = session.inventory.add(leftover)
                    if (left > 0) dropStack(session, leftover.copy(count = left))
                }
            }
            smithingSlots.fill(null)
            openSmithing = false
        }
        // parity: `StonecutterMenu.removed`, which hands whatever was in the input back.
        if (openStonecutter) {
            stonecutterInput?.let { leftover ->
                val left = session.inventory.add(leftover)
                if (left > 0) dropStack(session, leftover.copy(count = left))
            }
            stonecutterInput = null
            stonecutterResult = null
            openStonecutter = false
        }
        if (closing != null && closingBlock != null) {
            announceOpeners(
                closing,
                closingBlock,
                ContainerOpeners.close(session.worldName, closing.x, closing.y, closing.z),
            )
            closingSecond?.let { other ->
                announceOpeners(
                    other,
                    closingBlock,
                    ContainerOpeners.close(session.worldName, other.x, other.y, other.z),
                )
            }
        }
        openedAt = null
        recomputeCraftingResult()
        sendInventory()
    }

    /** Which container is open, by name, so closing one knows what lid it is shutting. */
    private fun openContainerBlock(pos: BlockPos): String? {
        val world = owner.currentWorld ?: return null
        val stateId = world.state.getBlockState(pos.x, pos.y, pos.z) ?: return null
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        return path.takeIf { it in Containers.PLAIN }
    }

    /** Reads one slot of the open window, whatever it refers to. */
    private fun readSlot(
        session: PlayerSession,
        slot: MenuSlot,
    ): StoredItemStack? =
        when (slot) {
            is MenuSlot.Inventory -> session.inventory[slot.index]
            is MenuSlot.Grid -> craftingGrid.getOrNull(slot.index)
            is MenuSlot.Block -> readBlockSlot(slot.index)
            MenuSlot.Result -> craftingResult
        }

    /** Writes one slot of the open window. The result slot is computed, never assigned. */
    private fun writeSlot(
        session: PlayerSession,
        slot: MenuSlot,
        stack: StoredItemStack?,
    ) {
        when (slot) {
            is MenuSlot.Inventory -> session.inventory[slot.index] = stack
            is MenuSlot.Grid -> {
                if (slot.index in craftingGrid.indices) craftingGrid[slot.index] = stack
                recomputeCraftingResult()
            }
            is MenuSlot.Block -> writeBlockSlot(slot.index, stack)
            MenuSlot.Result -> Unit
        }
    }

    /**
     * Whether a stack may be put into this slot at all.
     *
     * parity: `FurnaceResultSlot`, which takes nothing: the result slot of a window only ever gives,
     * and so does the cooked one.
     */
    private fun canPlaceInto(slot: MenuSlot): Boolean =
        when {
            slot == MenuSlot.Result -> false
            // parity: `FurnaceResultSlot.mayPlace`, which is a *furnace's* third slot and nobody
            // else's: a chest's third slot took nothing until this stopped being unconditional.
            slot is MenuSlot.Block &&
                openBlockSize == ClientSession.FURNACE_SLOTS &&
                slot.index == Furnaces.RESULT_SLOT -> false
            // parity: `StonecutterMenu.ResultSlot.mayPlace`, which is always false.
            slot is MenuSlot.Block && openStonecutter && slot.index == Menu.STONECUTTER_RESULT -> false
            // parity: `SmithingMenu`'s result slot, the same.
            slot is MenuSlot.Block && openSmithing && slot.index == Menu.SMITHING_RESULT -> false
            else -> true
        }

    /**
     * Recomputes what the grid makes. Called after anything touches it, which is what makes the
     * result appear and disappear as the player lays ingredients out.
     */
    private fun recomputeCraftingResult() {
        val recipes = owner.recipes
        if (recipes == null) {
            craftingResult = null
            return
        }
        val width = openMenu.gridWidth
        val height = openMenu.gridHeight
        val cells =
            List(width * height) { index ->
                // The components go into the grid too: a dye, a copy or a repair is defined by what
                // it does to them, so a recipe that cannot see them cannot exist.
                craftingGrid.getOrNull(index)?.let {
                    LootItemStack(Identifier.parse(it.id), it.count, it.components)
                }
            }
        val match = recipes.matchFor(CraftingInput.of(width, height, cells))
        // parity: the `limited_crafting` game rule, which is what makes `/recipe give` and
        // `/recipe take` mean something: with it on, a player crafts only what they have been
        // taught.
        val limited = owner.currentWorld?.gameRules?.get(GameRuleRegistry.LIMITED_CRAFTING) == true
        val known =
            !limited ||
                match?.first?.let {
                    owner.progress
                        ?.knownRecipes(
                            owner.playerSession
                                ?.uuid
                                ?.toString()
                                .orEmpty(),
                        )?.contains(it)
                } ==
                true
        craftingResult =
            match
                ?.takeIf { known }
                ?.second
                ?.let { StoredItemStack(it.item.toString(), it.count, it.components) }
    }

    /** What each grid cell is left holding, from the recipe that is about to be taken. */
    private fun craftingRemainders(): List<StoredItemStack?> {
        val recipes = owner.recipes ?: return emptyList()
        val width = openMenu.gridWidth
        val height = openMenu.gridHeight
        val cells =
            List(width * height) { index ->
                craftingGrid.getOrNull(index)?.let {
                    LootItemStack(Identifier.parse(it.id), it.count, it.components)
                }
            }
        val input = CraftingInput.of(width, height, cells)
        val recipe = recipes.recipeMatching(input)?.second ?: return emptyList()
        // The grid is trimmed to its occupied box, so the recipe answers in that box's order: walk
        // the same cells in the same order to line the two up.
        val answered = recipe.remainingItems(input).iterator()
        return List(width * height) { index ->
            if (craftingGrid.getOrNull(index) == null) {
                null
            } else if (answered.hasNext()) {
                answered.next()?.let { StoredItemStack(it.item.toString(), it.count, it.components) }
            } else {
                null
            }
        }
    }

    /**
     * Taking the result: hands over one batch and consumes one item from every occupied grid cell.
     *
     * parity: `ResultSlot.onTake`. Crafting is only ever committed here, when the player actually
     * takes it, so looking at a result costs nothing and closing the window mid-recipe loses no
     * ingredients.
     */
    private fun takeCraftingResult(session: PlayerSession) {
        val result = craftingResult ?: return
        val carried = carriedItem
        if (carried != null) {
            // The cursor has to be empty or hold the same thing, and have room for a whole batch.
            if (carried.id != result.id || carried.components != result.components) return
            if (carried.count + result.count > PlayerInventory.MAX_STACK) return
            carriedItem = carried.copy(count = carried.count + result.count)
        } else {
            carriedItem = result
        }
        // parity: `Recipe.getRemainingItems`: a cell whose item leaves something behind (a bucket, a
        // bottle) keeps it instead of emptying, and the book being copied stays where it was.
        val remaining = craftingRemainders()
        for (index in 0 until openMenu.gridSize) {
            val cell = craftingGrid.getOrNull(index) ?: continue
            val left = cell.count - 1
            val leftover = remaining.getOrNull(index)
            craftingGrid[index] =
                when {
                    left > 0 -> cell.copy(count = left)
                    leftover != null -> leftover
                    else -> null
                }
        }
        recomputeCraftingResult()
    }

    /**
     * Empties the crafting grid back into the player, dropping whatever will not fit.
     *
     * parity: `AbstractContainerMenu.removed`/`clearContainer`. Ingredients left on the grid when a
     * window closes are never destroyed, which is the whole reason this exists.
     */
    internal fun returnCraftingGrid(session: PlayerSession) {
        for (index in craftingGrid.indices) {
            val cell = craftingGrid[index] ?: continue
            craftingGrid[index] = null
            val leftover = session.inventory.add(cell)
            if (leftover > 0) dropStack(session, cell.copy(count = leftover))
        }
        // The cursor goes the same way: it is not part of the inventory either.
        carriedItem?.let { held ->
            carriedItem = null
            val leftover = session.inventory.add(held)
            if (leftover > 0) dropStack(session, held.copy(count = leftover))
        }
        craftingResult = null
    }

    /**
     * A click inside the player's own window.
     *
     * // parity: AbstractContainerMenu.doClick, reduced to the modes that move items between slots.
     * The cursor is the server's ([carriedItem]), not the client's predicted one, so claiming to
     * hold a stack cannot duplicate it: the click is re-derived and the window sent back.
     *
     * The 2x2 crafting grid and its result slot are not modelled: [PlayerInventory.storageSlotOf]
     * answers `null` there, so those clicks change nothing.
     */
    internal fun handleContainerClick(packet: ServerboundContainerClickPacket) {
        val session = owner.playerSession ?: return
        if (packet.containerId != openMenu.containerId) return sendInventory()
        val clicked =
            owner.playerEvent(PluginEventTypes.INVENTORY_CLICK) { event ->
                event.set(3, packet.slot)
                event.set(4, packet.button)
                event.set(5, packet.mode.name.lowercase())
                // What is in the slot the click is about, when it is one this build models.
                event.set(
                    6,
                    (openMenu.slotAt(packet.slot) as? MenuSlot.Inventory)?.let { session.inventory[it.index]?.id },
                )
            }
        if (clicked?.cancelled == true) {
            clicked.cancelMessage?.let { owner.sendPluginNotice(it) }
            // The client moved the item on its own screen the moment it clicked.
            sendInventory()
            return
        }

        // A plugin's window: the click is a button press, so nothing moves and the plugin is told
        // which of *its* slots was meant, never the player's own.
        val menu = pluginMenu
        if (menu != null) {
            (openMenu.slotAt(packet.slot) as? MenuSlot.Block)?.let { pluginMenuClick?.invoke(it.index) }
            sendInventory()
            return
        }

        when (packet.mode) {
            ServerboundContainerClickPacket.Mode.PICKUP -> {
                // Slot -999 is the click outside the window: whatever is on the cursor is thrown.
                if (packet.slot == ClientSession.OUTSIDE_WINDOW_SLOT) {
                    val carried = carriedItem ?: return sendInventory()
                    val thrown = if (packet.button == ClientSession.RIGHT_BUTTON) 1 else carried.count
                    dropStack(session, carried.copy(count = thrown))
                    val left = carried.count - thrown
                    carriedItem = if (left <= 0) null else carried.copy(count = left)
                    return sendInventory()
                }
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                if (slot == MenuSlot.Result) {
                    takeCraftingResult(session)
                } else {
                    pickupClick(session, slot, half = packet.button == ClientSession.RIGHT_BUTTON)
                }
            }
            ServerboundContainerClickPacket.Mode.SWAP -> {
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                if (!canPlaceInto(slot)) return sendInventory()
                if (packet.button !in 0 until PlayerInventory.HOTBAR_SIZE) return sendInventory()
                val hotbar = MenuSlot.Inventory(PlayerInventory.HOTBAR_START + packet.button)
                val fromSlot = readSlot(session, slot)
                writeSlot(session, slot, readSlot(session, hotbar))
                writeSlot(session, hotbar, fromSlot)
            }
            ServerboundContainerClickPacket.Mode.THROW -> {
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                if (slot == MenuSlot.Result) {
                    // Throwing the result still has to craft it, ingredients and all.
                    val previouslyCarried = carriedItem
                    carriedItem = null
                    takeCraftingResult(session)
                    carriedItem?.let { dropStack(session, it) }
                    carriedItem = previouslyCarried
                } else {
                    val stack = readSlot(session, slot) ?: return sendInventory()
                    // Button 0 is `Q`, button 1 is ctrl+`Q`: one item or the whole stack.
                    val thrown = if (packet.button == 0) 1 else stack.count
                    dropStack(session, stack.copy(count = thrown))
                    val left = stack.count - thrown
                    writeSlot(session, slot, if (left <= 0) null else stack.copy(count = left))
                }
            }
            ServerboundContainerClickPacket.Mode.QUICK_MOVE -> {
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                quickMove(session, slot)
            }
            ServerboundContainerClickPacket.Mode.QUICK_CRAFT -> {
                // Mid-gesture stages answer with nothing: resending the window while the player is
                // still dragging is what makes the drag snap back under their cursor.
                if (!handleQuickCraft(session, packet)) return
            }
            ServerboundContainerClickPacket.Mode.CLONE -> {
                // parity: creative's own middle-click, and only with an empty cursor.
                if (session.gamemode != Gamemode.CREATIVE || carriedItem != null) return sendInventory()
                val slot = openMenu.slotAt(packet.slot) ?: return sendInventory()
                val stack = readSlot(session, slot) ?: return sendInventory()
                carriedItem = stack.copy(count = PlayerInventory.MAX_STACK)
            }
            ServerboundContainerClickPacket.Mode.PICKUP_ALL -> {
                collectMatching(session, reversed = packet.button != 0)
            }
        }
        sendInventory()
    }

    /**
     * Double-click: sweeps matching stacks off the inventory and onto the cursor until it is full.
     *
     * parity: `AbstractContainerMenu.doClick`'s PICKUP_ALL branch, two passes and all. The first
     * pass deliberately skips already-full stacks so the loose ones are consumed first, which is
     * what makes double-clicking tidy an inventory up instead of just shuffling full stacks
     * around; the second pass takes whatever is still needed.
     */
    private fun collectMatching(
        session: PlayerSession,
        reversed: Boolean,
    ) {
        val carried = carriedItem ?: return
        var total = carried.count
        if (total >= PlayerInventory.MAX_STACK) return

        // Every slot of the *open window*, not just the player's own inventory: a crafting table
        // shows the grid too, and vanilla's own sweep gathers from everything the player can see.
        // Sweeping the inventory alone is why this did nothing useful with a table open.
        val slots =
            (0 until openMenu.slotCount)
                .mapNotNull(openMenu::slotAt)
                .filter { it != MenuSlot.Result }
                .let { if (reversed) it.reversed() else it }

        for (pass in 0..1) {
            for (slot in slots) {
                if (total >= PlayerInventory.MAX_STACK) break
                val existing = readSlot(session, slot) ?: continue
                if (existing.id != carried.id || existing.components != carried.components) continue
                if (pass == 0 && existing.count >= PlayerInventory.MAX_STACK) continue
                val taken = minOf(PlayerInventory.MAX_STACK - total, existing.count)
                total += taken
                val left = existing.count - taken
                writeSlot(session, slot, if (left <= 0) null else existing.copy(count = left))
            }
        }
        carriedItem = carried.copy(count = total)
    }

    /**
     * The click-and-drag gesture, in its three stages. Returns whether to send the window back:
     * only the last stage changes anything, and answering the others fights the drag in progress.
     *
     * // parity: AbstractContainerMenu.doClick's QUICK_CRAFT branch, bit layout included
     * (`getQuickcraftHeader` is `mask and 3`, `getQuickcraftType` is `mask shr 2 and 3`, the
     * opposite way round from how it reads), and its even share, single item or full stack split.
     * The client sends this for most drag-and-drop, so ignoring it left every later click acting
     * on a cursor the player had let go of.
     */
    private fun handleQuickCraft(
        session: PlayerSession,
        packet: ServerboundContainerClickPacket,
    ): Boolean {
        val stage = packet.button and 3
        val type = (packet.button shr 2) and 3
        val carried = carriedItem

        when {
            stage == ClientSession.QUICK_CRAFT_START -> {
                quickCraftType = type
                quickCraftSlots.clear()
                quickCraftActive = carried != null
                return false
            }
            stage == ClientSession.QUICK_CRAFT_ADD && quickCraftActive -> {
                val slot = openMenu.slotAt(packet.slot) ?: return false
                if (!canPlaceInto(slot)) return false
                val existing = readSlot(session, slot)
                val stackable =
                    carried != null &&
                        (existing == null || (existing.id == carried.id && existing.components == carried.components))
                if (stackable) quickCraftSlots += packet.slot
                return false
            }
            stage == ClientSession.QUICK_CRAFT_END && quickCraftActive -> {
                distributeQuickCraft(session)
                quickCraftActive = false
                quickCraftSlots.clear()
                return true
            }
            else -> {
                // Any other order is an interrupted gesture: forget it and resync.
                quickCraftActive = false
                quickCraftSlots.clear()
                return true
            }
        }
    }

    /** Spreads the cursor stack over the slots the drag collected. See [handleQuickCraft]. */
    private fun distributeQuickCraft(session: PlayerSession) {
        val carried = carriedItem ?: return
        val targets = quickCraftSlots.mapNotNull(openMenu::slotAt).filter(::canPlaceInto)
        if (targets.isEmpty()) return

        // parity: a one-slot drag is just a click on that slot, cursor rules and all.
        if (targets.size == 1) {
            pickupClick(session, targets.single(), half = quickCraftType == ClientSession.QUICK_CRAFT_TYPE_RIGHT)
            return
        }

        val perSlot =
            when (quickCraftType) {
                ClientSession.QUICK_CRAFT_TYPE_LEFT -> carried.count / targets.size
                ClientSession.QUICK_CRAFT_TYPE_RIGHT -> 1
                else -> PlayerInventory.MAX_STACK
            }
        if (perSlot <= 0) return

        var remaining = carried.count
        for (slot in targets) {
            if (remaining <= 0) break
            val already = readSlot(session, slot)?.count ?: 0
            val give = minOf(perSlot, PlayerInventory.MAX_STACK - already, remaining)
            if (give <= 0) continue
            writeSlot(session, slot, carried.copy(count = already + give))
            remaining -= give
        }
        // A middle-button drag is creative's own "fill them all" and costs the player nothing.
        carriedItem =
            when {
                quickCraftType == ClientSession.QUICK_CRAFT_TYPE_MIDDLE && session.gamemode == Gamemode.CREATIVE ->
                    carried
                remaining <= 0 -> null
                else -> carried.copy(count = remaining)
            }
    }

    /**
     * Left/right click on a slot, against the cursor. parity: `AbstractContainerMenu.doClick`'s own
     * PICKUP branch: an empty cursor takes the slot (half of it on a right click), a loaded cursor
     * merges into a matching stack or swaps with a different one.
     */
    private fun pickupClick(
        session: PlayerSession,
        slot: MenuSlot,
        half: Boolean,
    ) {
        val inSlot = readSlot(session, slot)
        val carried = carriedItem
        when {
            carried == null -> {
                if (inSlot == null) return
                val taken = if (half) (inSlot.count + 1) / 2 else inSlot.count
                carriedItem = inSlot.copy(count = taken)
                val left = inSlot.count - taken
                writeSlot(session, slot, if (left <= 0) null else inSlot.copy(count = left))
            }
            !canPlaceInto(slot) -> Unit
            inSlot == null -> {
                val placed = if (half) 1 else carried.count
                writeSlot(session, slot, carried.copy(count = placed))
                val left = carried.count - placed
                carriedItem = if (left <= 0) null else carried.copy(count = left)
            }
            inSlot.id == carried.id && inSlot.components == carried.components -> {
                val room = PlayerInventory.MAX_STACK - inSlot.count
                if (room <= 0) return
                val moved = if (half) minOf(1, room, carried.count) else minOf(room, carried.count)
                writeSlot(session, slot, inSlot.copy(count = inSlot.count + moved))
                val left = carried.count - moved
                carriedItem = if (left <= 0) null else carried.copy(count = left)
            }
            else -> {
                // Two different items: they trade places, cursor for slot.
                writeSlot(session, slot, carried)
                carriedItem = inSlot
            }
        }
    }

    /**
     * Shift-click. parity: `Inventory`'s own halves, so a stack in the hotbar goes up into the
     * storage rows and one in the rows comes down to the hotbar, merging into matching stacks first.
     */
    private fun quickMove(
        session: PlayerSession,
        slot: MenuSlot,
    ) {
        // Shift-clicking the result crafts as many times as the grid allows, which is what makes it
        // the fast way to turn a stack of logs into planks.
        if (slot == MenuSlot.Result) {
            val previouslyCarried = carriedItem
            carriedItem = null
            while (craftingResult != null) {
                val batch = craftingResult ?: break
                if (session.inventory.add(batch) > 0) break
                carriedItem = null
                takeCraftingResult(session)
                carriedItem = null
            }
            carriedItem = previouslyCarried
            return
        }

        val stack = readSlot(session, slot) ?: return
        // parity: `AbstractContainerMenu.quickMoveStack`, whose whole job is picking the other side:
        // with a block open, the inventory sends to the block and the block sends to the inventory.
        // Without a block open the two halves of the inventory swap, hotbar to storage and back.
        val intoBlock = slot is MenuSlot.Inventory && hasBlockSlots()
        val targets: List<MenuSlot> =
            when {
                intoBlock -> (0 until openBlockSize).map { MenuSlot.Block(it) }
                slot !is MenuSlot.Inventory ->
                    (
                        (PlayerInventory.HOTBAR_START until PlayerInventory.MAIN_SLOTS) +
                            (0 until PlayerInventory.HOTBAR_START)
                    ).map { MenuSlot.Inventory(it) }
                slot.index < PlayerInventory.HOTBAR_START ->
                    (PlayerInventory.HOTBAR_START until PlayerInventory.MAIN_SLOTS).map { MenuSlot.Inventory(it) }
                else -> (0 until PlayerInventory.HOTBAR_START).map { MenuSlot.Inventory(it) }
            }

        var remaining = stack.count
        // Stacking onto what is already there comes first, exactly as vanilla's `moveItemStackTo`.
        for (target in targets) {
            if (remaining <= 0) break
            if (!accepts(target, stack)) continue
            val existing = readSlot(session, target) ?: continue
            if (existing.id != stack.id || existing.components != stack.components) continue
            val room = PlayerInventory.MAX_STACK - existing.count
            if (room <= 0) continue
            val moved = minOf(room, remaining)
            writeSlot(session, target, existing.copy(count = existing.count + moved))
            remaining -= moved
        }
        for (target in targets) {
            if (remaining <= 0) break
            if (!accepts(target, stack)) continue
            if (readSlot(session, target) != null) continue
            writeSlot(session, target, stack.copy(count = remaining))
            remaining = 0
        }
        writeSlot(session, slot, if (remaining <= 0) null else stack.copy(count = remaining))
    }

    /** Whether the open screen has slots of its own to shift-click into. */
    private fun hasBlockSlots(): Boolean = openBlock != null || openEnderChest || openStonecutter || openSmithing

    /**
     * parity: `Slot.mayPlace`, which is why a shift-clicked log lands in a furnace's fuel slot and
     * not on top of what is being smelted.
     */
    private fun accepts(
        target: MenuSlot,
        stack: StoredItemStack,
    ): Boolean {
        if (target !is MenuSlot.Block) return true
        // parity: `StonecutterMenu.quickMoveStack` only ever moves into the input.
        if (openStonecutter) return target.index == Menu.STONECUTTER_INPUT
        // parity: `SmithingMenu.quickMoveStack`, which tries the three inputs and never the result.
        if (openSmithing) return target.index != Menu.SMITHING_RESULT
        if (openBlockSize != ClientSession.FURNACE_SLOTS) return true
        val item = runCatching { Identifier.parse(stack.id) }.getOrNull() ?: return false
        return when (target.index) {
            Furnaces.INPUT_SLOT -> true
            Furnaces.FUEL_SLOT -> Fuels.isFuel(item)
            // Nothing goes into the slot the cooked thing comes out of.
            else -> false
        }
    }

    /** Throws a stack into the world from this player, the same arc `Q` uses. */
    private fun dropStack(
        session: PlayerSession,
        stack: StoredItemStack,
    ) {
        val yaw = session.yaw * PI / 180.0
        val pitch = session.pitch * PI / 180.0
        val tossAngle = Random.nextDouble() * 2 * PI
        val tossSpread = ClientSession.TOSS_SPREAD * Random.nextDouble()
        owner.spawnDroppedItem(
            item = Identifier.parse(stack.id),
            count = stack.count,
            x = session.x,
            y = session.y + ClientSession.THROW_EYE_HEIGHT,
            z = session.z,
            velocityX = -sin(yaw) * cos(pitch) * ClientSession.THROW_SPEED + cos(tossAngle) * tossSpread,
            velocityY = -sin(pitch) * ClientSession.THROW_SPEED + ClientSession.THROW_LIFT + owner.tossLift(),
            velocityZ = cos(yaw) * cos(pitch) * ClientSession.THROW_SPEED + sin(tossAngle) * tossSpread,
        )
    }

    /**
     * The components of a stack the client sent back, as this build stores them.
     *
     * A creative client echoes a slot whenever the inventory screen opens, so anything not rebuilt
     * here is stripped off an item its owner merely looked at. What it cannot rebuild is what this
     * server never sent in the first place, since a client has nothing else to echo.
     */
    private fun componentsOf(wire: WireItemStack): NbtCompound =
        ItemComponentCodecs.toStored(wire.components) { owner.enchantmentNameOf(it) }

    /**
     * Serverbound, id 0x38: creative inventory editing, and the only path a dragged item takes,
     * discarding it as slot `-1` included. Never the window-click packet.
     *
     * Ignored outside creative, as `ServerPlayerGameMode.handleCreativeModeItemAdd` does.
     */

    internal fun handleSetCreativeModeSlot(packet: ServerboundSetCreativeModeSlotPacket) {
        val session = owner.playerSession ?: return
        if (session.gamemode != Gamemode.CREATIVE) return
        // The client numbers this in container 0's own slots, not this server's storage order, and
        // the two are neither the same size nor the same layout: taking it at face value wrote a
        // hotbar edit into the armour slots, so the item was accepted and then never in hand.
        // Slot -1 (dragged out of the window to discard) and the crafting slots resolve to null.
        val slot = PlayerInventory.storageSlotOf(packet.slot) ?: return
        val stack =
            packet.item?.let { wire ->
                val id = Items.REGISTRY.identifierOf(wire.itemId) ?: return@let null
                StoredItemStack(id.toString(), wire.count, componentsOf(wire))
            }
        session.inventory[slot] = stack
        sendInventorySlot(slot)
    }

    /**
     * Middle-click "pick block". In creative, always fills the current hotbar slot with a full
     * stack of the targeted block's item; in survival, only swaps it in from wherever this player
     * already carries it, matching vanilla: survival pick-block cannot conjure an item from
     * nothing. [ServerboundPickItemFromBlockPacket.includeData], the control-click that copies a
     * sign's text or a container's contents onto the picked stack, is a documented gap.
     */
    internal fun handlePickItemFromBlock(packet: ServerboundPickItemFromBlockPacket) {
        val session = owner.playerSession ?: return
        val stateId = owner.world.getBlockState(packet.pos.x, packet.pos.y, packet.pos.z) ?: return
        if (owner.isAirLike(stateId)) return
        val block = Blocks.byStateId(stateId) ?: return
        // parity: `Block.asItem`, which is not the block's own name for dust, crops or cocoa.
        val item = Items.REGISTRY.byIdentifier(ItemBlocks.itemOf(block.block)) ?: return
        val picking =
            owner.playerEvent(PluginEventTypes.PLAYER_PICK_ITEM) { event ->
                event.set(3, packet.pos.x)
                event.set(4, packet.pos.y)
                event.set(5, packet.pos.z)
                event.set(6, item.id.toString())
            }
        if (picking?.cancelled == true) {
            picking.cancelMessage?.let { owner.sendPluginNotice(it) }
            return
        }
        pickItem(session, item.id.toString())
    }

    /**
     * Middle-click "pick block" on an entity.
     *
     * parity: `handlePickItemFromEntity` over `Entity.getPickResult`: a mob answers with its own
     * spawn egg, a boat or a minecart with the item it was placed from, and everything else with
     * nothing at all. The control-click that copies the entity's data is ignored, as it is for a
     * block: there are no item data components here yet.
     */
    internal fun handlePickItemFromEntity(packet: ServerboundPickItemFromEntityPacket) {
        val session = owner.playerSession ?: return
        val entity = owner.currentWorld?.state?.entity(packet.entityId) ?: return
        if (entity.removed) return
        if (!owner.withinAttackReach(session, entity)) return
        val item = EntityPickResults.itemFor(entity.type) ?: return
        pickItem(session, item.toString())
    }

    /**
     * Puts [id] in the player's hand, however vanilla gets it there.
     *
     * parity: `tryPickItem`: already in the hotbar means only the selection moves, elsewhere in the
     * inventory means a swap rather than a copy, and conjuring one from nothing is creative only.
     */
    private fun pickItem(
        session: PlayerSession,
        id: String,
    ) {
        val inventory = session.inventory

        val existing = inventory.indexOf(id)
        when {
            // Already in the hotbar: vanilla only moves the selection, it does not touch a slot.
            existing != null && existing >= PlayerInventory.HOTBAR_START -> {
                inventory.heldSlot = existing - PlayerInventory.HOTBAR_START
            }
            // Elsewhere in the inventory: swapped into a suitable hotbar slot, not duplicated.
            existing != null -> {
                inventory.heldSlot = inventory.suitableHotbarSlot()
                inventory.swap(PlayerInventory.HOTBAR_START + inventory.heldSlot, existing)
            }
            // Not carried at all: only creative may conjure one, and only ever a single item.
            session.gamemode == Gamemode.CREATIVE -> {
                inventory.heldSlot = inventory.suitableHotbarSlot()
                val target = PlayerInventory.HOTBAR_START + inventory.heldSlot
                // Every hotbar slot was full, so whatever is in the way moves to a free slot
                // rather than being destroyed; if nothing is free, vanilla overwrites it too.
                inventory[target]?.let { displaced -> inventory.firstFreeSlot()?.let { inventory[it] = displaced } }
                inventory[target] = StoredItemStack(id, 1)
            }
            else -> return
        }

        owner.sendCompressed(SetHeldSlotPacket.ID) { SetHeldSlotPacket(inventory.heldSlot).write(it) }
        sendInventoryAll()
    }

    /**
     * The whole inventory, and which slot is in hand.
     *
     * Also the answer to a click: this build has no click handling, so a click is refused by
     * re-sending the window with a fresh state id. The client drops whatever it was predicting and
     * matches the server again, which is a read-only inventory rather than a desynchronised one.
     */
    internal fun sendInventory() {
        val session = owner.playerSession ?: return
        val state = ++containerStateId
        owner.sendCompressed(ContainerSetContentPacket.ID) {
            ContainerSetContentPacket(
                containerId = openMenu.containerId,
                stateId = state,
                items =
                    List(openMenu.slotCount) { menuSlot ->
                        openMenu.slotAt(menuSlot)?.let { session.inventory.toWire(readSlot(session, it)) }
                    },
                carriedItem = carriedItem?.let(session.inventory::toWire),
            ).write(it)
        }
        owner.sendCompressed(SetHeldSlotPacket.ID) { SetHeldSlotPacket(session.inventory.heldSlot).write(it) }
    }

    /** Re-sends the whole window, for a change that could have touched any slot. */
    internal fun sendInventoryAll() {
        sendInventory()
        owner.uiState.syncHeldAttributes()
    }

    /**
     * One slot changed, for when re-sending forty of them would be silly. [slot] is this server's
     * own storage index; it goes out translated to container 0's own numbering (see
     * [PlayerInventory.menuSlotOf]), because the client addresses its window, not our array.
     */
    internal fun sendInventorySlot(slot: Int) {
        val session = owner.playerSession ?: return
        if (slot == PlayerInventory.HOTBAR_START + session.inventory.heldSlot) owner.uiState.syncHeldAttributes()
        owner.sendPlayPacket(ContainerSetSlotPacket.ID) {
            ContainerSetSlotPacket(
                containerId = openMenu.containerId,
                stateId = ++containerStateId,
                slot = openMenu.menuSlotOf(MenuSlot.Inventory(slot)) ?: PlayerInventory.menuSlotOf(slot),
                item = session.inventory.toWire(slot),
            ).write(it)
        }
    }
}
