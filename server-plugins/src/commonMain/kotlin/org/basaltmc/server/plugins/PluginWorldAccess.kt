package org.basaltmc.server.plugins

/**
 * A player, as much of one as a plugin is allowed to touch.
 *
 * A handle rather than a copy: reading `health` reads the player's health now, and writing it
 * changes it. The handle stops working when they leave, and says so ([online]) rather than quietly
 * writing into nothing.
 */
public interface PluginPlayerRef {
    public val name: String
    public val uuid: String
    public val online: Boolean

    public var health: Double
    public var food: Int
    public var gamemode: String
    public var level: Int

    public val world: String
    public val x: Double
    public val y: Double
    public val z: Double
    public val ping: Int
    public val sneaking: Boolean

    /** MiniMessage, like every other piece of text a plugin writes. */
    public fun send(message: String)

    public fun actionBar(message: String)

    public fun teleport(
        world: String,
        x: Double,
        y: Double,
        z: Double,
    ): Boolean

    public fun kick(reason: String)

    /** Against the server's own groups, so a plugin does not keep a second permission system. */
    public fun may(permission: String): Boolean

    public fun give(
        item: String,
        count: Int,
    ): Boolean

    /** What they are holding, as `minecraft:stone`, or null for an empty hand. */
    public val heldItem: String?

    /** Their 41 slots. Still answers when they are offline, by doing nothing. */
    public val inventory: PluginInventory

    /** The big text in the middle. Empty strings leave that half of it alone. */
    public fun title(
        title: String,
        subtitle: String,
        fadeIn: Int,
        stay: Int,
        fadeOut: Int,
    )

    /** A sound at their own position, which is what a plugin wants nine times out of ten. */
    public fun sound(
        sound: String,
        volume: Float,
        pitch: Float,
    )

    public fun particle(
        particle: String,
        x: Double,
        y: Double,
        z: Double,
        count: Int,
        spread: Double,
    )

    /**
     * An attribute of theirs by its registry name, or null if there is no attribute by that name.
     *
     * The server's own, not a value drawn on the client: setting `max_health` moves what heals clamp
     * to and what damage measures against, not only how many hearts are painted.
     */
    public fun attribute(name: String): Double?

    public fun setAttribute(
        name: String,
        value: Double,
    ): Boolean

    public fun attributes(): List<String>
}

/**
 * One stack, as a plugin sees it: a value rather than a handle.
 *
 * The one place the object model goes the other way, because a stack has no identity of its own. It
 * is whatever is in a slot right now, so changing one is putting a different one in the slot.
 */
public data class PluginItem(
    public val id: String,
    public val count: Int = 1,
    /** The custom name, as MiniMessage, or null for whatever the item is called by default. */
    public val name: String? = null,
    public val lore: List<String> = emptyList(),
    public val damage: Int = 0,
    /** Enchantment id to level: `minecraft:sharpness` to 3. */
    public val enchantments: Map<String, Int> = emptyMap(),
)

/**
 * The slots of somebody, addressed by number.
 *
 * Slot numbers are vanilla's, because they are what the client clicks and what an event carries:
 * 0 to 35 is the main inventory with the hotbar at 27 to 35, 36 to 39 is armour and 40 is the
 * offhand. Renumbering them to something friendlier would only mean a plugin reading one number in
 * an event and passing a different one back.
 */
public interface PluginInventory {
    public val size: Int

    /** Which hotbar slot is in hand, 0 to 8. */
    public val heldSlot: Int

    public fun get(slot: Int): PluginItem?

    /** Puts [item] in [slot], or empties it with null. */
    public fun set(
        slot: Int,
        item: PluginItem?,
    )

    /** Adds it the way picking something up does. Answers how many did **not** fit. */
    public fun add(item: PluginItem): Int

    /** Takes up to [count] of [id] out. Answers how many were actually taken. */
    public fun remove(
        id: String,
        count: Int,
    ): Int

    public fun count(id: String): Int

    public fun clear()
}

/** Anything alive that is not a player: a mob, an item on the ground, a projectile. */
public interface PluginEntityRef {
    public val id: Int
    public val type: String
    public val world: String
    public val x: Double
    public val y: Double
    public val z: Double
    public var health: Double
    public val alive: Boolean

    public fun remove()

    public fun teleport(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean
}

/** Who is connected, and who was. */
public interface PluginPlayers {
    public fun online(): List<PluginPlayerRef>

    public fun byName(name: String): PluginPlayerRef?

    public fun byUuid(uuid: String): PluginPlayerRef?
}

/** One world, and the blocks in it. */
public interface PluginWorldRef {
    public val name: String

    /** Ticks since the world began, which is what the clock and the day count are made of. */
    public val time: Long

    public val raining: Boolean

    public val thundering: Boolean

    public val difficulty: String

    /** `minecraft:stone`, or null when that chunk is not loaded: a plugin should know the difference. */
    public fun block(
        x: Int,
        y: Int,
        z: Int,
    ): String?

    public fun setBlock(
        x: Int,
        y: Int,
        z: Int,
        block: String,
    ): Boolean

    public fun entities(): List<PluginEntityRef>

    /**
     * Floating text at a position, as a real `text_display` entity.
     *
     * Not an armour stand with a name and not a packet sent by hand: 26.2 has an entity for this, so
     * a hologram is one, which is why it survives a chunk reload and shows up in `/data get entity`
     * like anything else in the world.
     */
    public fun hologram(
        text: String,
        x: Double,
        y: Double,
        z: Double,
    ): PluginHologramRef?

    /**
     * A player-shaped entity standing here, which is what an NPC is.
     *
     * [onClick] is called with the uuid of whoever right-clicked it.
     */
    public fun npc(
        name: String,
        x: Double,
        y: Double,
        z: Double,
        onClick: ((String) -> Unit)?,
    ): PluginNpcRef?

    /**
     * Spawns a mob and hands it back, or null if the type is not one.
     *
     * This goes through the same path a natural spawn does, so `entity.spawn` fires with the reason
     * `plugin` and a plugin that refuses it refuses this too.
     */
    public fun spawn(
        type: String,
        x: Double,
        y: Double,
        z: Double,
    ): PluginEntityRef?

    /**
     * A gamerule of this world, as `/gamerule` prints it, or null if there is no rule by that name.
     *
     * The server's own rules, not a copy: a plugin that turns off `doDaylightCycle` turns off the
     * one the world is ticking, and an operator sees it in `/gamerule` like any other change.
     */
    public fun gameRule(name: String): String?

    /** Answers false if there is no such rule, or if the value does not fit its type. */
    public fun setGameRule(
        name: String,
        value: String,
    ): Boolean

    public fun gameRules(): List<String>
}

/**
 * Floating text somebody put in the world.
 *
 * An entity like any other, plus the two things a hologram is for: text that can be changed without
 * making a new one, and being shown to some people and not others.
 */
public interface PluginHologramRef : PluginEntityRef {
    /** MiniMessage, changed as often as you like: the entity stays the same one. */
    public var text: String

    /** Shows it only to these players, by uuid. An empty list hides it from everybody. */
    public fun showOnlyTo(uuids: List<String>)

    public fun showToAll()

    /** Who it is shown to, or null when that is everybody. */
    public val audience: List<String>?
}

/**
 * A player-shaped thing a plugin put somewhere.
 *
 * A real player entity, with a profile and a skin, because this is the server: there are no faked
 * packets here and nothing to detect about the client's version.
 */
public interface PluginNpcRef {
    public val id: Int
    public val uuid: String
    public val world: String
    public val x: Double
    public val y: Double
    public val z: Double
    public val alive: Boolean

    public var name: String

    /** The skin, as the value and signature of a profile property, which is how skins travel. */
    public fun skin(
        texture: String,
        signature: String?,
    )

    public fun look(
        yaw: Float,
        pitch: Float,
    )

    public fun teleport(
        x: Double,
        y: Double,
        z: Double,
    ): Boolean

    public fun showOnlyTo(uuids: List<String>)

    public fun showToAll()

    public val audience: List<String>?

    public fun remove()
}

/** The worlds this server has. */
public interface PluginWorlds {
    public fun all(): List<PluginWorldRef>

    public fun byName(name: String): PluginWorldRef?

    public val default: PluginWorldRef?

    /**
     * Makes a world that was not in `worlds.toml`.
     *
     * It is a world like any other: its own folder, its own `settings.toml` inheriting the defaults,
     * its own seed. Null if the name is taken or unusable. [dimension] is `overworld`, `nether` or
     * `the_end`, and [type] is `normal`, `flat`, `amplified` or `void`.
     */
    public fun create(
        name: String,
        dimension: String,
        type: String,
        seed: Long?,
    ): PluginWorldRef?
}

/**
 * What a plugin can put its own name on: placeholders the rest of the server already renders.
 *
 * Registering here rather than in a registry of the plugin's own is the point: `%arena_wins%` works
 * in the MOTD, in the sidebar and in another plugin's message, because there is one registry.
 */
public interface PluginPlaceholders {
    /**
     * Registers `%<pluginId>_<name>%`, answered by [value] for whoever it is rendered against.
     *
     * The player is null when it is rendered without one (the server list, a console message).
     */
    public fun register(
        pluginId: String,
        name: String,
        value: (PluginPlayerRef?) -> String,
    ): PluginHandle

    /** Renders every placeholder in [text] for [player], the same way the server's own files are. */
    public fun fill(
        text: String,
        player: PluginPlayerRef?,
    ): String
}
