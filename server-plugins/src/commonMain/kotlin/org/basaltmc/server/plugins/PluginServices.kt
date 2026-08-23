package org.basaltmc.server.plugins

/** Undoes one registration. Everything a plugin registers hands one of these back. */
public fun interface PluginHandle {
    public fun cancel()
}

/**
 * When plugin work runs.
 *
 * Everything here happens on an owner that is allowed to run plugins, so a plugin never has to
 * ask which thread it is on: it is always on one that may touch what it is about to touch.
 */
public interface PluginTasks {
    /** Ticks since the server started, for a plugin that needs to measure rather than wait. */
    public val tick: Long

    public fun after(
        delayTicks: Int,
        task: () -> Unit,
    ): PluginHandle

    public fun every(
        delayTicks: Int,
        periodTicks: Int,
        task: () -> Unit,
    ): PluginHandle
}

/** One run of a plugin command. */
public interface PluginCommandCall {
    /** Who ran it: a player's name, or the console's. */
    public val source: String

    public val isConsole: Boolean

    /** Answers in MiniMessage, which is the only text format the plugin API takes. */
    public fun reply(message: String)

    /** The parsed value, or null when an optional argument was left out. */
    public fun argument(name: String): Any?
}

/** How a plugin's commands reach the server's own dispatcher, rather than a second one beside it. */
public interface PluginCommands {
    public fun register(
        pluginId: String,
        spec: PluginCommandSpec,
        run: (PluginCommandCall) -> Unit,
    ): PluginHandle

    /** Runs one as the console, for a plugin the operator granted `commands`. Answers whether it ran. */
    public fun runAsConsole(command: String): Boolean
}

/**
 * Where the server keeps what belongs to plugins.
 *
 * One folder each, `plugins/<id>/`, and what is inside it is the plugin's business. The server does
 * not impose `config/`, `data/` or `lang/`: most plugins want none of that, and the ones that do
 * want it their own way. What the package ships under `resources/` is what the folder starts as,
 * with the same shape.
 */
public interface PluginPaths {
    /** Where the packages and the folders being worked on live. */
    public val packages: String

    /** One plugin's folder: its configuration, its data, and whatever else it decided to have. */
    public fun folder(pluginId: String): String
}

/**
 * What a plugin keeps: about a player, and about itself.
 *
 * Namespaced by the plugin's id at this seam rather than inside the plugin, so there is no call a
 * plugin can make that names somebody else's namespace. It cannot reach the server's own tables at
 * all: this is the only door, and it has one room behind it.
 */
public interface PluginStorage {
    public fun playerValue(
        pluginId: String,
        uuid: String,
        key: String,
    ): String?

    public fun setPlayerValue(
        pluginId: String,
        uuid: String,
        key: String,
        value: String?,
    )

    public fun playerValues(
        pluginId: String,
        uuid: String,
    ): Map<String, String>

    /** The highest [key] across every player this plugin knows: what a file cannot answer. */
    public fun top(
        pluginId: String,
        key: String,
        limit: Int,
    ): List<Pair<String, String>>

    /**
     * Which plugins have rows here and how many, for the orphan check at startup.
     *
     * Not part of what a plugin can call: this is the server looking at its own database, and it
     * names every plugin, which is exactly what no plugin may see.
     */
    public fun namespaces(): Map<String, Long> = emptyMap()

    /** Deletes everything one plugin kept. Only `/plugins purge` calls this, and only when asked. */
    public fun purge(pluginId: String): Long = 0
}

/** Everything the host borrows from the server. The server implements it; the host never reaches back. */
public interface PluginServices {
    public val tasks: PluginTasks
    public val commands: PluginCommands
    public val paths: PluginPaths

    /** Who is connected. Empty until the object model of stage 3 is wired to a real server. */
    public val players: PluginPlayers

    /** The worlds and their blocks. */
    public val worlds: PluginWorlds

    /** The server's own placeholder registry, which a plugin adds to rather than replacing. */
    public val placeholders: PluginPlaceholders

    /** Where a plugin's own data goes, namespaced by its id. */
    public val storage: PluginStorage

    /** Windows a plugin draws. */
    public val menus: PluginMenus

    /** Sending a packet the rest of the API does not cover, and the names of the ones there are. */
    public val packets: PluginPacketSender

    /** Talking to a mod on the client, or to the proxy. */
    public val channels: PluginChannelSender

    /**
     * Runs [task] on the owner of a position, and answers whether it could be arranged.
     *
     * This is what `at(pos)` is built on: reaching a block that belongs to another owner is a task
     * for that owner rather than a lock taken here. With regions off it is the same owner, and the
     * whole thing is one queue entry.
     */
    public fun at(
        world: String,
        x: Double,
        z: Double,
        task: () -> Unit,
    ): Boolean
}
