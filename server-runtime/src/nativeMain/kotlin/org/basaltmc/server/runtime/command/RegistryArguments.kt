package org.basaltmc.server.runtime.command

import org.basaltmc.server.world.structure.StructureMirror
import org.basaltmc.server.world.structure.StructureRotation
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.protocol.play.DisplaySlot
import org.basaltmc.wire.protocol.play.TeamColor
import org.basaltmc.wire.text.Component

/**
 * A registry entry by name, declared the way vanilla declares it.
 *
 * The text it accepts is the same one [ResourceLocationArgument] accepts; what changes is what the
 * client is told. A `minecraft:resource_location` has no completions of its own and no fallback, so
 * an argument declared that way shows the player its syntax and nothing else, while the parser
 * vanilla uses either completes from the client's own copy of the registry or asks the server for
 * it. That is the whole reason this type exists.
 *
 * The `#tag` forms the `resource_or_tag` parsers accept are refused here: nothing behind these
 * commands resolves a tag yet, so they fail by name rather than silently matching nothing.
 */
public class RegistryArgument(
    override val wire: ArgumentParser,
) : ArgumentType<Identifier> {
    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Identifier = ResourceLocationArgument.parse(reader, source, environment)

    public companion object {
        /** `minecraft:resource`: an entry that has to be loaded right now. */
        public fun resource(registry: String): RegistryArgument = RegistryArgument(ArgumentParser.Resource(registry))

        /** `minecraft:resource_key`: a name, whether or not anything is loaded under it. */
        public fun key(registry: String): RegistryArgument = RegistryArgument(ArgumentParser.ResourceKey(registry))

        public fun orTag(registry: String): RegistryArgument = RegistryArgument(ArgumentParser.ResourceOrTag(registry))

        public fun orTagKey(registry: String): RegistryArgument =
            RegistryArgument(ArgumentParser.ResourceOrTagKey(registry))

        /** `minecraft:resource_selector`: a name or a pattern matching several. */
        public fun selector(registry: String): RegistryArgument =
            RegistryArgument(ArgumentParser.ResourceSelector(registry))

        /** `minecraft:function`: a function or a `#tag` of them. */
        public val FUNCTION: RegistryArgument = RegistryArgument(ArgumentParser.Function)

        /** `minecraft:dialog`: a dialog id, or an inline dialog the client parses itself. */
        public val DIALOG: RegistryArgument = RegistryArgument(ArgumentParser.Dialog)
    }
}

/** `minecraft:objective`: the name of a scoreboard objective, which the client knows already. */
public object ObjectiveArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.Objective

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = reader.readString()
}

/** `minecraft:objective_criteria`: what an objective counts. */
public object ObjectiveCriteriaArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.ObjectiveCriteria

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = reader.readString()
}

/** `minecraft:team`: the name of a team, which the client also keeps. */
public object TeamArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.Team

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = reader.readString()
}

/** `minecraft:item_slot`: one inventory slot by its vanilla name. */
public object ItemSlotArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.ItemSlot

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = reader.readString()
}

/** `minecraft:int_range`: `3`, `3..`, `..7` or `3..7`, kept as written for the command to read. */
public object IntRangeArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.IntRange

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String {
        val start = reader.cursor
        while (reader.canRead() && !reader.peek().isWhitespace()) reader.skip()
        val text = reader.string.substring(start, reader.cursor)
        if (text.isEmpty()) {
            reader.cursor = start
            throw reader.syntaxError("Expected value or range of values")
        }
        return text
    }
}

/** `minecraft:hex_color`: `#rrggbb`, which is what a waypoint's own colour takes. */
public object HexColorArgument : ArgumentType<Int> {
    override val wire: ArgumentParser = ArgumentParser.HexColor

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Int {
        val start = reader.cursor
        while (reader.canRead() && !reader.peek().isWhitespace()) reader.skip()
        val text = reader.string.substring(start, reader.cursor).removePrefix("#")
        return text.toIntOrNull(16) ?: run {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.translatable("argument.color.invalid", Component.text(text)),
                reader.string,
                reader.cursor,
            )
        }
    }
}

/** `minecraft:team_color`: one of the sixteen chat colours a team can be painted with. */
public object TeamColorArgument : ArgumentType<TeamColor> {
    override val wire: ArgumentParser = ArgumentParser.TeamColor

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): TeamColor {
        val start = reader.cursor
        val name = reader.readUnquotedString()
        return TeamColor.entries.firstOrNull { it.name.equals(name, ignoreCase = true) }
            ?: run {
                reader.cursor = start
                throw CommandSyntaxException(
                    Component.translatable("argument.color.invalid", Component.text(name)),
                    reader.string,
                    reader.cursor,
                )
            }
    }
}

/** `minecraft:scoreboard_slot`: where an objective is drawn. */
public object ScoreboardSlotArgument : ArgumentType<DisplaySlot> {
    override val wire: ArgumentParser = ArgumentParser.ScoreboardSlot

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): DisplaySlot {
        val start = reader.cursor
        val name = reader.readUnquotedString()
        return DisplaySlot.entries.firstOrNull { it.name.equals(name, ignoreCase = true) }
            ?: run {
                reader.cursor = start
                // parity: vanilla also takes `sidebar.team.<colour>`, which needs per-team display
                // slots this build has not got. Refused by name rather than drawn in the wrong slot.
                throw CommandSyntaxException(
                    Component.translatable("argument.scoreboardDisplaySlot.invalid", Component.text(name)),
                    reader.string,
                    reader.cursor,
                )
            }
    }
}

/** `minecraft:template_rotation`: how `/place template` turns what it places. */
public object TemplateRotationArgument : ArgumentType<StructureRotation> {
    override val wire: ArgumentParser = ArgumentParser.TemplateRotation

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): StructureRotation {
        val start = reader.cursor
        val name = reader.readUnquotedString()
        return StructureRotation.entries.firstOrNull { it.serializedName == name }
            ?: run {
                reader.cursor = start
                throw reader.syntaxError("Invalid rotation '$name'")
            }
    }
}

/** `minecraft:template_mirror`: which way `/place template` flips what it places. */
public object TemplateMirrorArgument : ArgumentType<StructureMirror> {
    override val wire: ArgumentParser = ArgumentParser.TemplateMirror

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): StructureMirror {
        val start = reader.cursor
        val name = reader.readUnquotedString()
        return StructureMirror.entries.firstOrNull { it.serializedName == name }
            ?: run {
                reader.cursor = start
                throw reader.syntaxError("Invalid mirror '$name'")
            }
    }
}
