package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Items
import org.basaltmc.server.registry.ParticleType
import org.basaltmc.server.registry.ParticleTypes
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtByte
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.protocol.play.ParticleOptionShape
import org.basaltmc.wire.protocol.play.ParticleOptions
import org.basaltmc.wire.text.Component

/** A particle as `/particle` names it: the type, and the options it carries. */
public data class ParsedParticle(
    val type: ParticleType,
    val options: ParticleOptions?,
)

/**
 * `minecraft:particle`: a type id and, for the types that take one, an SNBT block of options.
 *
 * parity: `ParticleArgument.readParticle`, which reads the id and then hands whatever compound
 * follows to that type's own codec. The field names here are those codecs' own (`block_state`,
 * `color`, `from_color`, `water_blocks`), because they are what a command writes and what a
 * datapack already contains.
 */
public object ParticleArgument : ArgumentType<ParsedParticle> {
    override val wire: ArgumentParser = ArgumentParser.Particle

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): ParsedParticle {
        val start = reader.cursor
        val id = Identifier.parse(reader.readIdentifier())
        val type =
            ParticleTypes[id] ?: run {
                reader.cursor = start
                throw CommandSyntaxException(
                    Component.translatable("particle.notFound", Component.text(id.toString())),
                    reader.string,
                    reader.cursor,
                )
            }
        val compound =
            if (reader.canRead() && reader.peek() == '{') {
                NbtCompoundArgument.parse(reader, source, environment)
            } else {
                NbtCompound()
            }
        return ParsedParticle(type, optionsOf(type, compound, reader))
    }

    private fun optionsOf(
        type: ParticleType,
        options: NbtCompound,
        reader: StringReader,
    ): ParticleOptions? =
        when (type.optionShape) {
            ParticleOptionShape.NONE -> null
            ParticleOptionShape.BLOCK_STATE ->
                ParticleOptions.BlockState(blockState(options["block_state"], reader))
            ParticleOptionShape.COLOR -> ParticleOptions.Color(color(options["color"], reader))
            // parity: both fields are optional, defaulting to -1 (white) and a power of one.
            ParticleOptionShape.SPELL ->
                ParticleOptions.Spell(
                    options["color"]?.let { color(it, reader) } ?: -1,
                    number(options["power"]) ?: 1f,
                )
            ParticleOptionShape.DUST ->
                ParticleOptions.Dust(color(options["color"], reader), number(options["scale"]) ?: 1f)
            ParticleOptionShape.DUST_TRANSITION ->
                ParticleOptions.DustTransition(
                    color(options["from_color"], reader),
                    color(options["to_color"], reader),
                    number(options["scale"]) ?: 1f,
                )
            ParticleOptionShape.ITEM -> item(options["item"], reader)
            ParticleOptionShape.TRAIL -> trail(options, reader)
            ParticleOptionShape.NUMBER ->
                ParticleOptions.Number(number(options["roll"]) ?: number(options["power"]) ?: 1f)
            ParticleOptionShape.DELAY -> ParticleOptions.Delay(integer(options["delay"]) ?: 0)
            ParticleOptionShape.GEYSER ->
                ParticleOptions.Geyser(integer(options["water_blocks"]) ?: invalid(reader, "water_blocks"))
            ParticleOptionShape.GEYSER_BASE ->
                ParticleOptions.GeyserBase(
                    integer(options["water_blocks"]) ?: invalid(reader, "water_blocks"),
                    number(options["burst_impulse_base"]) ?: invalid(reader, "burst_impulse_base"),
                )
            ParticleOptionShape.VIBRATION -> vibration(options, reader)
        }

    /**
     * `block_state`, either a plain id or a `{Name:...}` compound.
     *
     * Block *properties* are dropped: this server registers every block by its default state only,
     * so a waterlogged or facing variant would resolve to the same number anyway.
     */
    private fun blockState(
        tag: NbtTag?,
        reader: StringReader,
    ): Int {
        val name =
            when (tag) {
                is NbtString -> tag.value
                is NbtCompound -> (tag["Name"] as? NbtString)?.value
                else -> null
            } ?: invalid(reader, "block_state")
        val block = Blocks.REGISTRY.byIdentifier(Identifier.parse(name)) ?: invalid(reader, name)
        return block.stateId
    }

    /** parity: `ExtraCodecs.RGB_COLOR_CODEC`, an int or a `[r,g,b]` of floats between zero and one. */
    private fun color(
        tag: NbtTag?,
        reader: StringReader,
    ): Int {
        integer(tag)?.let { return it }
        val list = tag as? NbtList ?: invalid(reader, "color")
        if (list.size < 3) invalid(reader, "color")
        val channels = (0 until list.size).map { number(list[it]) ?: 0f }
        val alpha = if (channels.size >= 4) channels[3] else 1f
        return packColor(alpha, channels[0], channels[1], channels[2])
    }

    /** parity: `ARGB.colorFromFloat`, which rounds each channel to a byte. */
    private fun packColor(
        alpha: Float,
        red: Float,
        green: Float,
        blue: Float,
    ): Int {
        fun channel(value: Float): Int = (value * 255f + 0.5f).toInt().coerceIn(0, 255)
        return (channel(alpha) shl 24) or (channel(red) shl 16) or (channel(green) shl 8) or channel(blue)
    }

    private fun item(
        tag: NbtTag?,
        reader: StringReader,
    ): ParticleOptions.Item {
        val (name, count) =
            when (tag) {
                is NbtString -> tag.value to 1
                is NbtCompound ->
                    ((tag["id"] as? NbtString)?.value ?: invalid(reader, "item")) to
                        (integer(tag["count"]) ?: 1)
                else -> invalid(reader, "item")
            }
        val item = Items.REGISTRY.byIdentifier(Identifier.parse(name)) ?: invalid(reader, name)
        return ParticleOptions.Item(item.protocolId, count)
    }

    private fun trail(
        options: NbtCompound,
        reader: StringReader,
    ): ParticleOptions.Trail {
        val target = options["target"] as? NbtList ?: invalid(reader, "target")
        if (target.size < 3) invalid(reader, "target")
        return ParticleOptions.Trail(
            targetX = (number(target[0]) ?: 0f).toDouble(),
            targetY = (number(target[1]) ?: 0f).toDouble(),
            targetZ = (number(target[2]) ?: 0f).toDouble(),
            color = color(options["color"], reader),
            duration = integer(options["duration"]) ?: invalid(reader, "duration"),
        )
    }

    private fun vibration(
        options: NbtCompound,
        reader: StringReader,
    ): ParticleOptions.Vibration {
        val destination = options["destination"] as? NbtCompound ?: invalid(reader, "destination")
        val pos = destination["pos"] as? NbtList ?: invalid(reader, "pos")
        if (pos.size < 3) invalid(reader, "pos")
        return ParticleOptions.Vibration(
            x = integer(pos[0]) ?: 0,
            y = integer(pos[1]) ?: 0,
            z = integer(pos[2]) ?: 0,
            arrivalTicks = integer(options["arrival_in_ticks"]) ?: 0,
        )
    }

    private fun number(tag: NbtTag?): Float? =
        when (tag) {
            is NbtFloat -> tag.value
            is NbtDouble -> tag.value.toFloat()
            is NbtInt -> tag.value.toFloat()
            is NbtByte -> tag.value.toFloat()
            else -> null
        }

    private fun integer(tag: NbtTag?): Int? = number(tag)?.toInt()

    private fun invalid(
        reader: StringReader,
        what: String,
    ): Nothing =
        throw CommandSyntaxException(
            Component.translatable("particle.invalidOptions", Component.text(what)),
            reader.string,
            reader.cursor,
        )
}
