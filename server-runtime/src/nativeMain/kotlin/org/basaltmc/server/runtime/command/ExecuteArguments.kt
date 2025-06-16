package org.basaltmc.server.runtime.command

import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.ArgumentParser

/** Where a position means: the feet or the eyes. parity: `EntityAnchorArgument`. */
public enum class EntityAnchor {
    FEET,
    EYES,
}

/** `minecraft:entity_anchor`: `feet` or `eyes`. */
public object EntityAnchorArgument : ArgumentType<EntityAnchor> {
    override val wire: ArgumentParser = ArgumentParser.EntityAnchor

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): EntityAnchor {
        val start = reader.cursor
        return when (reader.readUnquotedString()) {
            "feet" -> EntityAnchor.FEET
            "eyes" -> EntityAnchor.EYES
            else -> {
                reader.cursor = start
                throw reader.syntaxError("Invalid entity anchor")
            }
        }
    }
}

/** `minecraft:swizzle`: a combination of `x`, `y` and `z`, each at most once. parity: `SwizzleArgument`. */
public object SwizzleArgument : ArgumentType<Set<Char>> {
    override val wire: ArgumentParser = ArgumentParser.Swizzle

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Set<Char> {
        val start = reader.cursor
        val axes = mutableSetOf<Char>()
        while (reader.canRead() && reader.peek() != ' ') {
            val axis = reader.read()
            if (axis !in "xyz" || !axes.add(axis)) {
                reader.cursor = start
                throw reader.syntaxError("Invalid swizzle, expected combination of 'x', 'y' and 'z'")
            }
        }
        if (axes.isEmpty()) {
            reader.cursor = start
            throw reader.syntaxError("Invalid swizzle, expected combination of 'x', 'y' and 'z'")
        }
        return axes
    }
}

/** `minecraft:heightmap`: which surface `/execute positioned over` lands on. */
public object HeightmapArgument : ArgumentType<String> {
    private val NAMES = listOf("world_surface", "motion_blocking", "motion_blocking_no_leaves", "ocean_floor")

    override val wire: ArgumentParser = ArgumentParser.Heightmap

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String {
        val start = reader.cursor
        val name = reader.readUnquotedString()
        if (name !in NAMES) {
            reader.cursor = start
            throw reader.syntaxError("Unknown heightmap type '$name'")
        }
        return name
    }
}

/** `minecraft:item_slots`: one slot name or a group of them, kept as written. */
public object ItemSlotsArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.ItemSlots

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = reader.readString()
}

/** `minecraft:loot_predicate`: the id of a predicate to test. */
public object LootPredicateArgument : ArgumentType<Identifier> {
    override val wire: ArgumentParser = ArgumentParser.LootPredicate

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Identifier = ResourceLocationArgument.parse(reader, source, environment)
}

/**
 * `minecraft:loot_table`: the id of a loot table.
 *
 * Not a `minecraft:resource` over the `minecraft:loot_table` registry, which is what this used to
 * be: loot tables are a datapack registry and the client has no copy of one, so a real client threw
 * `Registry minecraft:loot_table not found` building the command tree and dropped the connection.
 */
public object LootTableArgument : ArgumentType<Identifier> {
    override val wire: ArgumentParser = ArgumentParser.LootTable

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Identifier = ResourceLocationArgument.parse(reader, source, environment)
}

/** `minecraft:loot_modifier`: the id of an item modifier, for the same reason as [LootTableArgument]. */
public object LootModifierArgument : ArgumentType<Identifier> {
    override val wire: ArgumentParser = ArgumentParser.LootModifier

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Identifier = ResourceLocationArgument.parse(reader, source, environment)
}

/** `minecraft:float_range`: `1`, `1..`, `..2` or `1..2`, kept as written. */
public object FloatRangeArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.FloatRange

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = IntRangeArgument.parse(reader, source, environment)
}

/** `minecraft:uuid`: an entity or profile id, kept as written. */
public object UuidArgument : ArgumentType<String> {
    private const val LENGTH = 36

    override val wire: ArgumentParser = ArgumentParser.Uuid

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String {
        val start = reader.cursor
        while (reader.canRead() && reader.peek() != ' ') reader.skip()
        val text = reader.string.substring(start, reader.cursor)
        if (text.length != LENGTH || text.count { it == '-' } != 4) {
            reader.cursor = start
            throw reader.syntaxError("Invalid UUID")
        }
        return text
    }
}

/** `minecraft:style`: the style half of a component, written as SNBT. */
public object StyleArgument : ArgumentType<NbtCompound> {
    override val wire: ArgumentParser = ArgumentParser.Style

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): NbtCompound = NbtCompoundArgument.parse(reader, source, environment)
}
