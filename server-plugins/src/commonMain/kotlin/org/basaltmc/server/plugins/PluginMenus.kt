package org.basaltmc.server.plugins

/**
 * A window a plugin draws and a player clicks, with none of the slot arithmetic.
 *
 * The reason this exists rather than leaving plugins to build one out of a chest and
 * `InventoryClickEvent` is that everyone writes the same four bugs doing that: the item is taken
 * because the click was not cancelled, the numbers are off because the player's own inventory shares
 * the window's numbering, the menu survives a reload, and two menus of the same plugin get confused
 * for each other. Here a menu is a thing, its slots are its own, and a click is answered with who
 * clicked and where.
 */
public interface PluginMenus {
    /**
     * Shows [player] a window of [rows] rows of nine, filled from [items] by slot.
     *
     * Nothing in it can be taken: a menu is a set of buttons, and a plugin that wants a container
     * people put things into wants a chest, which is a different thing. [onClick] is called with the
     * slot that was clicked, on the server's side of the window, and the window is redrawn after.
     */
    public fun open(
        player: PluginPlayerRef,
        title: String,
        rows: Int,
        items: Map<Int, PluginItem>,
        onClick: (Int) -> Unit,
    ): PluginHandle

    /** Shuts whatever this player has open, as if they pressed escape. */
    public fun close(player: PluginPlayerRef)

    /** Redraws an open menu after its items changed. */
    public fun refresh(
        player: PluginPlayerRef,
        items: Map<Int, PluginItem>,
    )

    public companion object {
        /** A window is rows of nine, and the client draws at most six of them. */
        public const val COLUMNS: Int = 9
        public const val MAX_ROWS: Int = 6
    }
}
