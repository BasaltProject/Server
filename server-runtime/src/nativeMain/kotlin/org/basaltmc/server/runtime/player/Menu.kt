package org.basaltmc.server.runtime.player

/** What a slot of an open window actually refers to. */
public sealed interface MenuSlot {
    /** A slot of the player's own inventory, in [PlayerInventory]'s storage numbering. */
    public data class Inventory(
        public val index: Int,
    ) : MenuSlot

    /** A cell of the crafting grid, row-major. */
    public data class Grid(
        public val index: Int,
    ) : MenuSlot

    /** The crafting result. Can be taken from, never placed into. */
    public data object Result : MenuSlot

    /**
     * A slot of the block the window belongs to: a furnace's input, fuel and result.
     *
     * The block keeps its own contents, so the session only says which of them a click meant.
     */
    public data class Block(
        public val index: Int,
    ) : MenuSlot
}

/**
 * The slot layout of one open window.
 *
 * The client addresses everything by its position in *this* layout, which is neither the server's
 * storage order nor the same between windows: the player's own inventory puts a 2x2 grid and the
 * armour where the crafting table puts a 3x3 grid, and both are 46 slots long. Getting this mapping
 * wrong does not fail loudly, it just moves the wrong item, so it lives in one place with both
 * layouts written out rather than being computed at each call site.
 *
 * parity: `InventoryMenu` and `CraftingMenu`, whose own slot order this mirrors.
 */
public class Menu(
    public val containerId: Int,
    public val gridWidth: Int,
    public val gridHeight: Int,
    private val slots: List<MenuSlot?>,
) {
    public val slotCount: Int get() = slots.size
    public val gridSize: Int get() = gridWidth * gridHeight

    /** What this window's slot refers to, or `null` for one this build does not model. */
    public fun slotAt(menuSlot: Int): MenuSlot? = slots.getOrNull(menuSlot)

    /** Where [slot] appears in this window, or `null` if this window does not show it. */
    public fun menuSlotOf(slot: MenuSlot): Int? = slots.indexOf(slot).takeIf { it >= 0 }

    public companion object {
        /** Both windows are this long; only what the slots mean differs. */
        private const val SLOT_COUNT = 46

        /**
         * The player's own window, always open, always container 0: result, 2x2 grid, armour,
         * storage rows, hotbar, offhand.
         */
        public fun playerInventory(): Menu =
            Menu(
                containerId = 0,
                gridWidth = 2,
                gridHeight = 2,
                slots =
                    List(SLOT_COUNT) { slot ->
                        when (slot) {
                            0 -> MenuSlot.Result
                            in 1..4 -> MenuSlot.Grid(slot - 1)
                            in 5..8 -> MenuSlot.Inventory(slot - 5 + 36)
                            in 9..35 -> MenuSlot.Inventory(slot - 9)
                            in 36..44 -> MenuSlot.Inventory(slot - 36 + PlayerInventory.HOTBAR_START)
                            45 -> MenuSlot.Inventory(40)
                            else -> null
                        }
                    },
            )

        /** How many slots a cooker's window has: three of its own, then the player's 36. */
        private const val COOKER_SLOT_COUNT = 39

        /** A single chest: 27 of its own, then the player's 36. parity: `ChestMenu.threeRows`. */
        private const val CHEST_SLOT_COUNT = 63

        /** parity: `ChestMenu`'s own three rows of nine. */
        public const val CHEST_SIZE: Int = 27

        /**
         * A chest, a trapped chest or a barrel: its own rows first, then the player's storage and
         * hotbar. The block keeps the contents, so a click only says which of its slots was meant.
         */
        public fun chest(containerId: Int): Menu =
            Menu(
                containerId = containerId,
                gridWidth = 0,
                gridHeight = 0,
                slots =
                    List(CHEST_SLOT_COUNT) { slot ->
                        when (slot) {
                            in 0 until CHEST_SIZE -> MenuSlot.Block(slot)
                            in CHEST_SIZE..53 -> MenuSlot.Inventory(slot - CHEST_SIZE)
                            in 54..62 -> MenuSlot.Inventory(slot - 54 + PlayerInventory.HOTBAR_START)
                            else -> null
                        }
                    },
            )

        /**
         * A window of [rows] rows of nine, then the player's own 36: `minecraft:generic_9xN`.
         *
         * What a plugin's menu is drawn in. The slots above the player's are [MenuSlot.Block] like
         * a chest's, because the *shape* is a chest's; what is behind them is not a block, which is
         * the caller's business and not this class's.
         */
        public fun rows(
            containerId: Int,
            rows: Int,
        ): Menu {
            val own = rows * COLUMNS
            return Menu(
                containerId = containerId,
                gridWidth = 0,
                gridHeight = 0,
                slots =
                    List(own + PLAYER_SLOTS_IN_WINDOW) { slot ->
                        when {
                            slot < own -> MenuSlot.Block(slot)
                            slot < own + STORAGE_SLOTS -> MenuSlot.Inventory(slot - own)
                            else ->
                                MenuSlot.Inventory(
                                    slot - own - STORAGE_SLOTS + PlayerInventory.HOTBAR_START,
                                )
                        }
                    },
            )
        }

        /** Nine to a row, which is every window the client draws. */
        public const val COLUMNS: Int = 9

        /** The player's own storage and hotbar, tacked onto the bottom of every container window. */
        private const val STORAGE_SLOTS = 27
        private const val PLAYER_SLOTS_IN_WINDOW = 36

        /** parity: `ChestMenu.sixRows`, the double chest: 54 of its own, then the player's 36. */
        public const val DOUBLE_CHEST_SIZE: Int = 54
        private const val DOUBLE_CHEST_SLOT_COUNT = 90

        /**
         * Two chests as one window. The first 27 slots are the *first* half as vanilla orders them,
         * which is not always the one that was clicked: see `ChestBlock.DOUBLE_INVENTORY_COMBINER`.
         */
        public fun doubleChest(containerId: Int): Menu =
            Menu(
                containerId = containerId,
                gridWidth = 0,
                gridHeight = 0,
                slots =
                    List(DOUBLE_CHEST_SLOT_COUNT) { slot ->
                        when (slot) {
                            in 0 until DOUBLE_CHEST_SIZE -> MenuSlot.Block(slot)
                            in DOUBLE_CHEST_SIZE..80 -> MenuSlot.Inventory(slot - DOUBLE_CHEST_SIZE)
                            in 81..89 -> MenuSlot.Inventory(slot - 81 + PlayerInventory.HOTBAR_START)
                            else -> null
                        }
                    },
            )

        /** parity: `StonecutterMenu`: one slot in, one out, then the player's 36. */
        private const val STONECUTTER_SLOT_COUNT = 38

        /** The stonecutter's two slots: what goes in, and what the chosen button makes of it. */
        public const val STONECUTTER_INPUT: Int = 0
        public const val STONECUTTER_RESULT: Int = 1

        public fun stonecutter(containerId: Int): Menu =
            Menu(
                containerId = containerId,
                gridWidth = 0,
                gridHeight = 0,
                slots =
                    List(STONECUTTER_SLOT_COUNT) { slot ->
                        when (slot) {
                            in 0..1 -> MenuSlot.Block(slot)
                            in 2..28 -> MenuSlot.Inventory(slot - 2)
                            in 29..37 -> MenuSlot.Inventory(slot - 29 + PlayerInventory.HOTBAR_START)
                            else -> null
                        }
                    },
            )

        /** parity: `SmithingMenu`: template, base, addition, result, then the player's 36. */
        private const val SMITHING_SLOT_COUNT = 40

        public const val SMITHING_TEMPLATE: Int = 0
        public const val SMITHING_BASE: Int = 1
        public const val SMITHING_ADDITION: Int = 2
        public const val SMITHING_RESULT: Int = 3

        public fun smithing(containerId: Int): Menu =
            Menu(
                containerId = containerId,
                gridWidth = 0,
                gridHeight = 0,
                slots =
                    List(SMITHING_SLOT_COUNT) { slot ->
                        when (slot) {
                            in 0..3 -> MenuSlot.Block(slot)
                            in 4..30 -> MenuSlot.Inventory(slot - 4)
                            in 31..39 -> MenuSlot.Inventory(slot - 31 + PlayerInventory.HOTBAR_START)
                            else -> null
                        }
                    },
            )

        /** parity: `MerchantMenu`: two slots to pay with, one to take from, then the player's 36. */
        private const val MERCHANT_SLOT_COUNT = 39

        /**
         * A villager's window. The three trading slots are the menu's own rather than any block's,
         * so nothing behind them is a container: the trade itself is what moves the items.
         */
        public fun merchant(containerId: Int): Menu =
            Menu(
                containerId = containerId,
                gridWidth = 0,
                gridHeight = 0,
                slots =
                    List(MERCHANT_SLOT_COUNT) { slot ->
                        when (slot) {
                            in 0..2 -> null
                            in 3..29 -> MenuSlot.Inventory(slot - 3)
                            in 30..38 -> MenuSlot.Inventory(slot - 30 + PlayerInventory.HOTBAR_START)
                            else -> null
                        }
                    },
            )

        /** parity: `CrafterMenu`: nine of the block's own, then the player's 36. */
        public const val CRAFTER_SIZE: Int = 9
        private const val CRAFTER_SLOT_COUNT = 45

        /**
         * A crafter: its nine slots, then the player's storage and hotbar. There is no result slot,
         * because nobody takes anything out of a crafter by hand: it pushes what it makes.
         */
        public fun crafter(containerId: Int): Menu =
            Menu(
                containerId = containerId,
                gridWidth = 0,
                gridHeight = 0,
                slots =
                    List(CRAFTER_SLOT_COUNT) { slot ->
                        when (slot) {
                            in 0 until CRAFTER_SIZE -> MenuSlot.Block(slot)
                            in CRAFTER_SIZE..35 -> MenuSlot.Inventory(slot - CRAFTER_SIZE)
                            in 36..44 -> MenuSlot.Inventory(slot - 36 + PlayerInventory.HOTBAR_START)
                            else -> null
                        }
                    },
            )

        /**
         * A furnace, a blast furnace or a smoker: input, fuel, result, then the player's storage and
         * hotbar. parity: `AbstractFurnaceMenu`'s own slot order.
         */
        public fun furnace(containerId: Int): Menu =
            Menu(
                containerId = containerId,
                gridWidth = 0,
                gridHeight = 0,
                slots =
                    List(COOKER_SLOT_COUNT) { slot ->
                        when (slot) {
                            in 0..2 -> MenuSlot.Block(slot)
                            in 3..29 -> MenuSlot.Inventory(slot - 3)
                            in 30..38 -> MenuSlot.Inventory(slot - 30 + PlayerInventory.HOTBAR_START)
                            else -> null
                        }
                    },
            )

        /**
         * A crafting table: result, 3x3 grid, then the player's storage and hotbar. No armour and no
         * offhand, which is exactly why its numbering cannot be shared with the window above.
         */
        public fun craftingTable(containerId: Int): Menu =
            Menu(
                containerId = containerId,
                gridWidth = 3,
                gridHeight = 3,
                slots =
                    List(SLOT_COUNT) { slot ->
                        when (slot) {
                            0 -> MenuSlot.Result
                            in 1..9 -> MenuSlot.Grid(slot - 1)
                            in 10..36 -> MenuSlot.Inventory(slot - 10)
                            in 37..45 -> MenuSlot.Inventory(slot - 37 + PlayerInventory.HOTBAR_START)
                            else -> null
                        }
                    },
            )
    }
}
