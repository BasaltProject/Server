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
