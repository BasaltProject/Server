package org.basaltmc.server.plugins

import net.blueva.luak.LuaError
import net.blueva.luak.LuaTable
import net.blueva.luak.LuaValue
import net.blueva.luak.Varargs
import org.basaltmc.server.lua.HostFunction
import org.basaltmc.server.lua.Lua
import org.basaltmc.server.lua.Lua.field
import kotlin.math.floor
import kotlin.math.sqrt

/**
 * An object of the server as Lua sees it: `p.health = p.health + 1` reads and writes the player who
 * is standing there.
 *
 * A handle rather than a copy, because a copy is a lie the moment anything moves. Reading a name the
 * object does not have is an error listing what it does have: a silent nil is a bug that shows up
 * three lines later.
 */
internal class LuaObject(
    private val kind: String,
    private val readers: Map<String, () -> Any?>,
    private val writers: Map<String, (LuaValue) -> Unit> = emptyMap(),
    private val methods: Map<String, HostFunction> = emptyMap(),
) {
    val table: LuaTable = Lua.table()

    init {
        val metatable = Lua.table()
        metatable.set("__index", HostFunction("$kind.index") { args -> read(args) })
        metatable.set("__newindex", HostFunction("$kind.newindex") { args -> write(args) })
        metatable.set("__tostring", HostFunction("$kind.tostring") { Lua.of(kind) })
        table.setmetatable(metatable)
    }

    private fun read(args: Varargs): Varargs {
        val key = args.arg(2).tojstring()
        methods[key]?.let { return it }
        val reader =
            readers[key]
                ?: throw LuaError(
                    "a $kind has no '$key'; it has ${(readers.keys + methods.keys).sorted().joinToString(", ")}",
                )
        return valueOf(reader())
    }

    private fun write(args: Varargs): Varargs {
        val key = args.arg(2).tojstring()
        val writer =
            writers[key]
                ?: throw LuaError(
                    if (key in readers) {
                        "'$key' of a $kind can be read but not changed"
                    } else {
                        "a $kind has no '$key' to change"
                    },
                )
        writer(args.arg(3))
        return Lua.NONE
    }

    private fun valueOf(value: Any?): LuaValue =
        when (value) {
            null -> Lua.NIL
            is String -> Lua.of(value)
            is Int -> Lua.of(value)
            is Long -> Lua.of(value)
            is Double -> Lua.of(value)
            is Float -> Lua.of(value.toDouble())
            is Boolean -> Lua.of(value)
            is LuaValue -> value
            is List<*> -> {
                val out = Lua.table()
                for ((index, element) in value.withIndex()) out.set(index + 1, valueOf(element))
                out
            }

            else -> Lua.of(value.toString())
        }
}

/**
 * `p.data.kills = (p.data.kills or 0) + 1`.
 *
 * A table whose reads and writes go to `players.db`, in this plugin's namespace: it looks like a
 * field of the player because that is what it is, and it survives them logging out, the world being
 * deleted and the server being restarted. Values are numbers, strings and booleans; anything else
 * is what `data.json` is for.
 */
internal fun playerDataTable(
    storage: PluginStorage,
    pluginId: String,
    uuid: String,
): LuaTable {
    val table = Lua.table()
    val metatable = Lua.table()
    metatable.set(
        "__index",
        HostFunction("data.index") { args ->
            val key = args.arg(2).tojstring()
            val stored = storage.playerValue(pluginId, uuid, key) ?: return@HostFunction Lua.NIL
            // Numbers come back as numbers: a plugin that wrote 7 should not have to remember it
            // is reading "7".
            stored.toLongOrNull()?.let { return@HostFunction Lua.of(it) }
            stored.toDoubleOrNull()?.let { return@HostFunction Lua.of(it) }
            when (stored) {
                "true" -> Lua.TRUE
                "false" -> Lua.FALSE
                else -> Lua.of(stored)
            }
        },
    )
    metatable.set(
        "__newindex",
        HostFunction("data.newindex") { args ->
            val key = args.arg(2).tojstring()
            val value = args.arg(3)
            storage.setPlayerValue(pluginId, uuid, key, if (value.isnil()) null else value.tojstring())
            Lua.NONE
        },
    )
    table.setmetatable(metatable)
    return table
}

/** Turns one player into the object a plugin holds. */
internal fun playerObject(
    player: PluginPlayerRef,
    storage: PluginStorage? = null,
    pluginId: String? = null,
    text: PluginText? = null,
): LuaTable =
    LuaObject(
        kind = "player",
        readers =
            mapOf(
                "name" to { player.name },
                "uuid" to { player.uuid },
                "online" to { player.online },
                "health" to { player.health },
                "food" to { player.food },
                "gamemode" to { player.gamemode },
                "level" to { player.level },
                "world" to { player.world },
                "x" to { player.x },
                "y" to { player.y },
                "z" to { player.z },
                "ping" to { player.ping },
                "sneaking" to { player.sneaking },
                "held_item" to { player.heldItem },
                "attributes" to { player.attributes() },
                "inventory" to { inventoryObject(player.inventory) },
                "pos" to { positionObject(player.world, player.x, player.y, player.z) },
                "data" to
                    {
                        if (storage != null && pluginId != null) {
                            playerDataTable(storage, pluginId, player.uuid)
                        } else {
                            null
                        }
                    },
            ),
        writers =
            mapOf(
                "health" to { value -> player.health = value.todouble() },
                "food" to { value -> player.food = value.toint() },
                "gamemode" to { value -> player.gamemode = value.tojstring() },
                "level" to { value -> player.level = value.toint() },
            ),
        methods =
            mapOf(
                "send" to
                    HostFunction("player.send") { args ->
                        player.send(resolved(text, args))
                        Lua.NONE
                    },
                "action_bar" to
                    HostFunction("player.action_bar") { args ->
                        player.actionBar(resolved(text, args))
                        Lua.NONE
                    },
                "title" to
                    HostFunction("player.title") { args ->
                        // p:title("<gold>Go") or p:title(title, subtitle, { fade_in = 5, stay = 40 }).
                        val base = firstArgument(args)
                        val options = args.arg(base + 2).takeIf { it.istable() }
                        player.title(
                            resolved(text, args),
                            args.arg(base + 1).takeIf { it.isstring() }?.let {
                                text?.resolve(it.tojstring(), Lua.NIL) ?: it.tojstring()
                            } ?: "",
                            fadeIn = options.timing("fade_in", FADE_IN),
                            stay = options.timing("stay", STAY),
                            fadeOut = options.timing("fade_out", FADE_OUT),
                        )
                        Lua.NONE
                    },
                "attribute" to
                    HostFunction("player.attribute") { args ->
                        // p:attribute("max_health") reads, p:attribute("max_health", 40) sets.
                        val base = firstArgument(args)
                        val name = args.checkjstring(base)
                        val given = args.arg(base + 1)
                        if (given.isnil()) {
                            player.attribute(name)?.let { Lua.of(it) } ?: Lua.NIL
                        } else {
                            Lua.of(player.setAttribute(name, given.todouble()))
                        }
                    },
                "sound" to
                    HostFunction("player.sound") { args ->
                        val base = firstArgument(args)
                        player.sound(
                            args.checkjstring(base),
                            args.optdouble(base + 1, 1.0).toFloat(),
                            args.optdouble(base + 2, 1.0).toFloat(),
                        )
                        Lua.NONE
                    },
                "particle" to
                    HostFunction("player.particle") { args ->
                        // p:particle("minecraft:flame", pos, count, spread), the position optional.
                        val base = firstArgument(args)
                        val name = args.checkjstring(base)
                        val point = pointFrom(args.arg(base + 1))
                        val next = if (point == null) base + 1 else base + 2
                        player.particle(
                            name,
                            point?.x ?: player.x,
                            point?.y ?: player.y,
                            point?.z ?: player.z,
                            count = args.optint(next, 1),
                            spread = args.optdouble(next + 1, 0.0),
                        )
                        Lua.NONE
                    },
                "kick" to
                    HostFunction("player.kick") { args ->
                        player.kick(resolved(text, args))
                        Lua.NONE
                    },
                "may" to
                    HostFunction("player.may") { args ->
                        Lua.of(player.may(args.tojstring(firstArgument(args))))
                    },
                "give" to
                    HostFunction("player.give") { args ->
                        val first = firstArgument(args)
                        Lua.of(player.give(args.tojstring(first), args.optint(first + 1, 1)))
                    },
                "teleport" to
                    HostFunction("player.teleport") { args ->
                        // p:teleport(pos), p:teleport(x, y, z) or p:teleport(world, x, y, z).
                        val first = firstArgument(args)
                        pointFrom(args.arg(first))?.let { point ->
                            return@HostFunction Lua.of(
                                player.teleport(point.world ?: player.world, point.x, point.y, point.z),
                            )
                        }
                        val named = args.arg(first).isstring() && !args.arg(first).isnumber()
                        val world = if (named) args.tojstring(first) else player.world
                        val base = if (named) first + 1 else first
                        Lua.of(
                            player.teleport(
                                world,
                                args.todouble(base),
                                args.todouble(base + 1),
                                args.todouble(base + 2),
                            ),
                        )
                    },
            ),
    ).table

internal fun entityObject(entity: PluginEntityRef): LuaTable =
    LuaObject(
        kind = "entity",
        readers =
            mapOf(
                "id" to { entity.id },
                "type" to { entity.type },
                "world" to { entity.world },
                "x" to { entity.x },
                "y" to { entity.y },
                "z" to { entity.z },
                "health" to { entity.health },
                "alive" to { entity.alive },
                "pos" to { positionObject(entity.world, entity.x, entity.y, entity.z) },
            ),
        writers = mapOf("health" to { value -> entity.health = value.todouble() }),
        methods =
            mapOf(
                "remove" to
                    HostFunction("entity.remove") {
                        entity.remove()
                        Lua.NONE
                    },
                "teleport" to
                    HostFunction("entity.teleport") { args ->
                        val base = firstArgument(args)
                        pointFrom(args.arg(base))?.let { point ->
                            return@HostFunction Lua.of(entity.teleport(point.x, point.y, point.z))
                        }
                        Lua.of(entity.teleport(args.todouble(base), args.todouble(base + 1), args.todouble(base + 2)))
                    },
            ),
    ).table

/** A player-shaped entity: the same handle a hologram has, plus a name, a skin and a look. */
internal fun npcObject(npc: PluginNpcRef): LuaTable =
    LuaObject(
        kind = "npc",
        readers =
            mapOf(
                "id" to { npc.id },
                "uuid" to { npc.uuid },
                "name" to { npc.name },
                "world" to { npc.world },
                "x" to { npc.x },
                "y" to { npc.y },
                "z" to { npc.z },
                "alive" to { npc.alive },
                "audience" to { npc.audience },
            ),
        writers = mapOf("name" to { value -> npc.name = value.tojstring() }),
        methods =
            mapOf(
                "skin" to
                    HostFunction("npc.skin") { args ->
                        // npc:skin(texture, signature) or npc:skin(player), which wears theirs.
                        val base = firstArgument(args)
                        val first = args.arg(base)
                        if (first.istable()) {
                            val who =
                                first
                                    .field("name")
                                    .takeIf { it.isstring() }
                                    ?.tojstring()
                                    .orEmpty()
                            return@HostFunction Lua.of(npc.skinFrom(who))
                        }
                        npc.skin(args.checkjstring(base), args.arg(base + 1).takeIf { it.isstring() }?.tojstring())
                        Lua.of(true)
                    },
                "skin_from" to
                    HostFunction("npc.skin_from") { args ->
                        Lua.of(npc.skinFrom(args.checkjstring(firstArgument(args))))
                    },
                "looks_at_players" to
                    HostFunction("npc.looks_at_players") { args ->
                        val base = firstArgument(args)
                        npc.looksAtPlayers(args.arg(base).isnil() || args.arg(base).toboolean())
                        Lua.NONE
                    },
                "look" to
                    HostFunction("npc.look") { args ->
                        val base = firstArgument(args)
                        npc.look(args.todouble(base).toFloat(), args.optdouble(base + 1, 0.0).toFloat())
                        Lua.NONE
                    },
                "teleport" to
                    HostFunction("npc.teleport") { args ->
                        val base = firstArgument(args)
                        pointFrom(args.arg(base))?.let { point ->
                            return@HostFunction Lua.of(npc.teleport(point.x, point.y, point.z))
                        }
                        Lua.of(npc.teleport(args.todouble(base), args.todouble(base + 1), args.todouble(base + 2)))
                    },
                "show_only_to" to
                    HostFunction("npc.show_only_to") { args ->
                        npc.showOnlyTo(uuidsFrom(args.arg(firstArgument(args))))
                        Lua.NONE
                    },
                "show_to_all" to
                    HostFunction("npc.show_to_all") {
                        npc.showToAll()
                        Lua.NONE
                    },
                "remove" to
                    HostFunction("npc.remove") {
                        npc.remove()
                        Lua.NONE
                    },
            ),
    ).table

/**
 * What goes over an NPC's head: one line or several.
 *
 * A list is joined with newlines, which is what a text display draws, so
 * `name = { "<gold>Shop", "<gray>right click" }` is two lines with colour in both. It is not the
 * profile name, which could hold neither.
 */
internal fun nameLines(value: LuaValue): String =
    when {
        value.isstring() -> value.tojstring()
        value.istable() -> (1..value.length()).joinToString("\n") { (value.get(it) ?: Lua.NIL).tojstring() }
        else -> ""
    }

/** Players or uuids, since a plugin has one or the other to hand. */
internal fun uuidsFrom(list: LuaValue): List<String> {
    if (!list.istable()) return emptyList()
    val out = mutableListOf<String>()
    for (index in 1..list.length()) {
        val at = list.get(index) ?: Lua.NIL
        when {
            at.istable() -> at.field("uuid").takeIf { it.isstring() }?.let { out += it.tojstring() }
            at.isstring() -> out += at.tojstring()
        }
    }
    return out
}

/** Floating text: an entity object with its text and who it is for. */
internal fun hologramObject(hologram: PluginHologramRef): LuaTable =
    LuaObject(
        kind = "hologram",
        readers =
            mapOf(
                "id" to { hologram.id },
                "type" to { hologram.type },
                "world" to { hologram.world },
                "x" to { hologram.x },
                "y" to { hologram.y },
                "z" to { hologram.z },
                "alive" to { hologram.alive },
                "text" to { hologram.text },
                "audience" to { hologram.audience },
            ),
        writers = mapOf("text" to { value -> hologram.text = value.tojstring() }),
        methods =
            mapOf(
                "remove" to
                    HostFunction("hologram.remove") {
                        hologram.remove()
                        Lua.NONE
                    },
                "teleport" to
                    HostFunction("hologram.teleport") { args ->
                        val base = firstArgument(args)
                        pointFrom(args.arg(base))?.let { point ->
                            return@HostFunction Lua.of(hologram.teleport(point.x, point.y, point.z))
                        }
                        Lua.of(hologram.teleport(args.todouble(base), args.todouble(base + 1), args.todouble(base + 2)))
                    },
                "show_only_to" to
                    HostFunction("hologram.show_only_to") { args ->
                        hologram.showOnlyTo(uuidsFrom(args.arg(firstArgument(args))))
                        Lua.NONE
                    },
                "show_to_all" to
                    HostFunction("hologram.show_to_all") {
                        hologram.showToAll()
                        Lua.NONE
                    },
            ),
    ).table

internal fun worldObject(
    world: PluginWorldRef,
    /** How a click on an NPC gets back into the plugin's own state, mediated like every callback. */
    onNpcClick: (LuaValue, String) -> Unit = { _, _ -> },
    /** `@key` and `{name}` work in a hologram and over an NPC's head, like anywhere else text goes. */
    resolveText: (String) -> String = { it },
): LuaTable =
    LuaObject(
        kind = "world",
        readers =
            mapOf(
                "name" to { world.name },
                "time" to { world.time },
                "raining" to { world.raining },
                "thundering" to { world.thundering },
                "difficulty" to { world.difficulty },
                "rules" to { world.gameRules() },
            ),
        methods =
            mapOf(
                "block" to
                    HostFunction("world.block") { args ->
                        // w:block(pos) or w:block(x, y, z); the second one is why this cannot use
                        // firstArgument, since three numbers and self are four arguments either way.
                        pointFrom(args.arg(if (args.narg() >= 2) 2 else 1))?.let { point ->
                            return@HostFunction world
                                .block(floor(point.x).toInt(), floor(point.y).toInt(), floor(point.z).toInt())
                                ?.let { Lua.of(it) } ?: Lua.NIL
                        }
                        val base = if (args.narg() >= 4) 2 else 1
                        world
                            .block(args.checkint(base), args.checkint(base + 1), args.checkint(base + 2))
                            ?.let { Lua.of(it) } ?: Lua.NIL
                    },
                "set_block" to
                    HostFunction("world.set_block") { args ->
                        val base = firstArgument(args)
                        pointFrom(args.arg(base))?.let { point ->
                            return@HostFunction Lua.of(
                                world.setBlock(
                                    floor(point.x).toInt(),
                                    floor(point.y).toInt(),
                                    floor(point.z).toInt(),
                                    args.checkjstring(base + 1),
                                ),
                            )
                        }
                        Lua.of(
                            world.setBlock(
                                args.checkint(base),
                                args.checkint(base + 1),
                                args.checkint(base + 2),
                                args.checkjstring(base + 3),
                            ),
                        )
                    },
                "rule" to
                    HostFunction("world.rule") { args ->
                        // w:rule("doDaylightCycle") reads it, w:rule("doDaylightCycle", false) sets it.
                        val base = firstArgument(args)
                        val name = args.checkjstring(base)
                        val given = args.arg(base + 1)
                        if (given.isnil()) {
                            world.gameRule(name)?.let { Lua.of(it) } ?: Lua.NIL
                        } else {
                            Lua.of(world.setGameRule(name, given.tojstring()))
                        }
                    },
                "entities" to
                    HostFunction("world.entities") {
                        val out = Lua.table()
                        for ((index, entity) in world.entities().withIndex()) {
                            out.set(index + 1, entityObject(entity))
                        }
                        out
                    },
                "npc" to
                    HostFunction("world.npc") { args ->
                        // w:npc{ name = "Bob", pos = ..., skin = ..., on_click = function(p) end }.
                        val options = args.arg(firstArgument(args))
                        if (!options.istable()) throw LuaError("world:npc{ name = ..., pos = ... } takes a table")
                        val point =
                            pointFrom(options.field("pos"))
                                ?: throw LuaError("an npc needs a pos: worlds.default():npc{ pos = p.pos }")
                        val handler = options.field("on_click").takeIf { it.isfunction() }
                        val made =
                            world.npc(
                                resolveText(nameLines(options.field("name"))),
                                point.x,
                                point.y,
                                point.z,
                                handler?.let { fn -> { uuid -> onNpcClick(fn, uuid) } },
                            ) ?: return@HostFunction Lua.NIL
                        options.field("skin").takeIf { it.istable() }?.let { skin ->
                            skin.field("texture").takeIf { it.isstring() }?.let { texture ->
                                made.skin(
                                    texture.tojstring(),
                                    skin.field("signature").takeIf { it.isstring() }?.tojstring(),
                                )
                            }
                        }
                        npcObject(made)
                    },
                "hologram" to
                    HostFunction("world.hologram") { args ->
                        // w:hologram("<gold>Shop", pos) or w:hologram(text, x, y, z).
                        val base = firstArgument(args)
                        val text = resolveText(args.checkjstring(base))
                        val point = pointFrom(args.arg(base + 1))
                        val made =
                            if (point != null) {
                                world.hologram(text, point.x, point.y, point.z)
                            } else {
                                world.hologram(
                                    text,
                                    args.todouble(base + 1),
                                    args.todouble(base + 2),
                                    args.todouble(base + 3),
                                )
                            }
                        made?.let { hologramObject(it) } ?: Lua.NIL
                    },
                "spawn" to
                    HostFunction("world.spawn") { args ->
                        val base = firstArgument(args)
                        val type = args.checkjstring(base)
                        pointFrom(args.arg(base + 1))?.let { point ->
                            return@HostFunction world
                                .spawn(type, point.x, point.y, point.z)
                                ?.let { entityObject(it) } ?: Lua.NIL
                        }
                        world
                            .spawn(
                                type,
                                args.todouble(base + 1),
                                args.todouble(base + 2),
                                args.todouble(base + 3),
                            )?.let { entityObject(it) } ?: Lua.NIL
                    },
            ),
    ).table

/**
 * A place: a world and three numbers, together, as a value rather than a handle.
 *
 * Passing `x, y, z` through four calls is where coordinates get swapped, and a plugin that wants to
 * remember somewhere should not have to invent its own table for it.
 */
internal fun positionObject(
    world: String,
    x: Double,
    y: Double,
    z: Double,
): LuaTable =
    LuaObject(
        kind = "position",
        readers =
            mapOf(
                "world" to { world },
                "x" to { x },
                "y" to { y },
                "z" to { z },
                // The chunk it falls in, which is what a claim or a region keys on.
                "chunk_x" to { floorDiv(x, CHUNK) },
                "chunk_z" to { floorDiv(z, CHUNK) },
            ),
        methods =
            mapOf(
                "offset" to
                    HostFunction("position.offset") { args ->
                        val base = firstArgument(args)
                        positionObject(
                            world,
                            x + args.optdouble(base, 0.0),
                            y + args.optdouble(base + 1, 0.0),
                            z + args.optdouble(base + 2, 0.0),
                        )
                    },
                "distance" to
                    HostFunction("position.distance") { args ->
                        val other =
                            pointFrom(args.arg(firstArgument(args)))
                                ?: throw LuaError("distance needs another position")
                        val dx = x - other.x
                        val dy = y - other.y
                        val dz = z - other.z
                        Lua.of(sqrt(dx * dx + dy * dy + dz * dz))
                    },
            ),
    ).table

private const val CHUNK = 16

private fun floorDiv(
    value: Double,
    by: Int,
): Int = floor(value / by).toInt()

/** A world and three numbers, however the plugin spelled them. */
internal class PluginPoint(
    val world: String?,
    val x: Double,
    val y: Double,
    val z: Double,
)

/** Reads a position out of a table that has `x`, `y` and `z`: our own or the plugin's own. */
internal fun pointFrom(value: LuaValue): PluginPoint? {
    if (!value.istable()) return null
    val x = value.field("x")
    if (!x.isnumber()) return null
    val world = value.field("world")
    return PluginPoint(
        world = if (world.isstring()) world.tojstring() else null,
        x = x.todouble(),
        y = value.field("y").todouble(),
        z = value.field("z").todouble(),
    )
}

/**
 * One stack in Lua. Read-only, because a stack is a value: to change it, put another one in the slot.
 */
internal fun itemObject(item: PluginItem): LuaTable =
    LuaObject(
        kind = "item",
        readers =
            mapOf(
                "id" to { item.id },
                "count" to { item.count },
                "name" to { item.name },
                "lore" to { item.lore },
                "damage" to { item.damage },
                "enchantments" to
                    {
                        val out = Lua.table()
                        for ((id, level) in item.enchantments) out.set(id, Lua.of(level))
                        out
                    },
            ),
    ).table

/**
 * What a plugin wrote where a stack was expected.
 *
 * `"minecraft:bread"` and `{ id = "minecraft:bread", count = 3 }` are both one, because asking for
 * a table to hand over a single item is the kind of ceremony this API exists to avoid.
 */
internal fun itemFrom(value: LuaValue): PluginItem? {
    if (value.isstring() && !value.isnumber()) return PluginItem(value.tojstring())
    if (!value.istable()) return null
    val id = value.field("id")
    if (!id.isstring()) throw LuaError("an item needs an 'id', as in { id = \"minecraft:bread\", count = 3 }")
    val count = value.field("count")
    val name = value.field("name")
    val damage = value.field("damage")
    return PluginItem(
        id = id.tojstring(),
        count = if (count.isnumber()) count.toint() else 1,
        name = if (name.isstring()) name.tojstring() else null,
        lore = stringsOf(value.field("lore")),
        damage = if (damage.isnumber()) damage.toint() else 0,
        enchantments = levelsOf(value.field("enchantments")),
    )
}

private fun stringsOf(value: LuaValue): List<String> {
    if (!value.istable()) return emptyList()
    val out = mutableListOf<String>()
    for (index in 1..value.length()) out += (value.get(index) ?: Lua.NIL).tojstring()
    return out
}

private fun levelsOf(value: LuaValue): Map<String, Int> {
    if (!value.istable()) return emptyMap()
    val out = mutableMapOf<String, Int>()
    var key: LuaValue = Lua.NIL
    while (true) {
        val pair = value.next(key) ?: break
        val nextKey = pair.arg1()
        if (nextKey.isnil()) break
        out[nextKey.tojstring()] = pair.arg(2).toint()
        key = nextKey
    }
    return out
}

/** The slots, as `inv:get(0)` and `inv:set(0, item)`. */
internal fun inventoryObject(inventory: PluginInventory): LuaTable =
    LuaObject(
        kind = "inventory",
        readers =
            mapOf(
                "size" to { inventory.size },
                "held_slot" to { inventory.heldSlot },
                "held" to { inventory.get(HOTBAR_START + inventory.heldSlot)?.let { itemObject(it) } },
            ),
        methods =
            mapOf(
                "get" to
                    HostFunction("inventory.get") { args ->
                        val base = firstArgument(args)
                        inventory.get(args.checkint(base))?.let { itemObject(it) } ?: Lua.NIL
                    },
                "set" to
                    HostFunction("inventory.set") { args ->
                        val base = firstArgument(args)
                        val slot = args.checkint(base)
                        val given = args.arg(base + 1)
                        inventory.set(slot, if (given.isnil()) null else itemFrom(given))
                        Lua.NONE
                    },
                "add" to
                    HostFunction("inventory.add") { args ->
                        // inv:add("minecraft:bread", 3) and inv:add({ ... }) are both one call.
                        val base = firstArgument(args)
                        val item =
                            itemFrom(args.arg(base))
                                ?: throw LuaError("inventory.add needs an item id or a table")
                        val count = args.arg(base + 1)
                        val wanted = if (count.isnumber()) item.copy(count = count.toint()) else item
                        Lua.of(inventory.add(wanted))
                    },
                "remove" to
                    HostFunction("inventory.remove") { args ->
                        val base = firstArgument(args)
                        Lua.of(inventory.remove(args.checkjstring(base), args.optint(base + 1, 1)))
                    },
                "count" to
                    HostFunction("inventory.count") { args ->
                        Lua.of(inventory.count(args.checkjstring(firstArgument(args))))
                    },
                "clear" to
                    HostFunction("inventory.clear") {
                        inventory.clear()
                        Lua.NONE
                    },
            ),
    ).table

/** Vanilla's own numbering, which is what an event carries and what a click indexes. */
private const val HOTBAR_START = 27

// parity: what /title uses when nobody says otherwise, in ticks.
private const val FADE_IN = 10
private const val STAY = 70
private const val FADE_OUT = 20

private fun LuaValue?.timing(
    key: String,
    fallback: Int,
): Int {
    val value = this?.field(key) ?: return fallback
    return if (value.isnumber()) value.toint() else fallback
}

/**
 * A window, and what happens when somebody clicks it.
 *
 * The slot numbers are the menu's own, 0 upwards, which is the one place this API deliberately does
 * not use vanilla's numbering: a menu is not a container, its rows are its own, and making a plugin
 * add 9 to reach the second row of its own window is exactly the arithmetic this removes.
 */
internal class LuaMenu(
    private val menus: PluginMenus,
    private val players: PluginPlayers,
    private val title: String,
    private val rows: Int,
    /** Runs a plugin's click handler on its own state, mediated the way every callback here is. */
    private val call: (LuaValue, PluginPlayerRef, Int) -> Unit,
) {
    private val items = linkedMapOf<Int, PluginItem>()
    private val handlers = linkedMapOf<Int, LuaValue>()
    private val opened = mutableMapOf<String, PluginHandle>()

    val table: LuaTable =
        LuaObject(
            kind = "menu",
            readers =
                mapOf(
                    "title" to { title },
                    "rows" to { rows },
                    "size" to { rows * PluginMenus.COLUMNS },
                ),
            methods =
                mapOf(
                    "set" to
                        HostFunction("menu.set") { args ->
                            val base = firstArgument(args)
                            val slot = args.checkint(base)
                            val item =
                                itemFrom(args.arg(base + 1))
                                    ?: throw LuaError("menu:set(slot, item) needs an item")
                            if (slot !in 0 until rows * PluginMenus.COLUMNS) {
                                throw LuaError("slot $slot is outside a menu of $rows rows")
                            }
                            items[slot] = item
                            val handler = args.arg(base + 2)
                            if (handler.isfunction()) handlers[slot] = handler else handlers.remove(slot)
                            redraw()
                            Lua.NONE
                        },
                    "clear" to
                        HostFunction("menu.clear") {
                            items.clear()
                            handlers.clear()
                            redraw()
                            Lua.NONE
                        },
                    "open" to
                        HostFunction("menu.open") { args ->
                            val player =
                                playerFrom(args.arg(firstArgument(args)), players)
                                    ?: throw LuaError("menu:open(player) needs a player")
                            opened[player.uuid]?.cancel()
                            opened[player.uuid] =
                                menus.open(player, title, rows, items.toMap()) { slot ->
                                    handlers[slot]?.let { call(it, player, slot) }
                                }
                            Lua.NONE
                        },
                    "close" to
                        HostFunction("menu.close") { args ->
                            val player =
                                playerFrom(args.arg(firstArgument(args)), players)
                                    ?: throw LuaError("menu:close(player) needs a player")
                            menus.close(player)
                            opened.remove(player.uuid)?.cancel()
                            Lua.NONE
                        },
                ),
        ).table

    /** Everyone looking at this menu sees the change, without the plugin reopening it. */
    private fun redraw() {
        for (uuid in opened.keys) players.byUuid(uuid)?.let { menus.refresh(it, items.toMap()) }
    }

    /** Shuts every copy of this menu: what unloading the plugin has to do. */
    fun closeAll() {
        for ((uuid, handle) in opened) {
            players.byUuid(uuid)?.let { menus.close(it) }
            handle.cancel()
        }
        opened.clear()
    }
}

/**
 * The player behind a player object, by the uuid it carries.
 *
 * Looked up rather than remembered: a player object in Lua is a table with a uuid in it, and the
 * registry is the one thing that knows whether that uuid is still somebody who is here.
 */
internal fun playerFrom(
    value: LuaValue,
    players: PluginPlayers,
): PluginPlayerRef? {
    if (!value.istable()) return null
    val uuid = value.field("uuid").takeIf { it.isstring() }?.tojstring() ?: return null
    return players.byUuid(uuid)
}

/** `p:send("text")` and `p.send("text")` both land here, and neither has to be the written one. */
private fun messageOf(args: Varargs): String = args.tojstring(firstArgument(args))

/** The message the plugin wrote, with its `@key` looked up and its `{name}` filled in. */
private fun resolved(
    text: PluginText?,
    args: Varargs,
): String {
    val first = firstArgument(args)
    val message = args.tojstring(first)
    return text?.resolve(message, args.arg(first + 1)) ?: message
}

/**
 * Where the real arguments start.
 *
 * `p:send(x)` passes the object as the first one and `p.send(x)` does not, and a plugin should not
 * have to know which it wrote. A table in the first slot is the object: none of these methods takes
 * one as its first real argument, which is what makes the rule safe rather than a guess about types.
 */
internal fun firstArgument(args: Varargs): Int = if (args.narg() >= 1 && args.arg(1).istable()) 2 else 1
