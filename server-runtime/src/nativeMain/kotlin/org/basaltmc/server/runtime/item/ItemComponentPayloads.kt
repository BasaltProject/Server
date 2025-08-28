package org.basaltmc.server.runtime.item

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.registry.Items
import org.basaltmc.server.runtime.registry.VanillaRegistryData
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.nbt.NbtByte
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtFloat
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.protocol.play.ItemComponent
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt

/**
 * The components whose network form is written straight into bytes rather than modelled as a type.
 *
 * They ride out as [ItemComponent.Raw] because the payload is already what a client expects, and a
 * class each would buy nothing: nothing here reads them back, since a stack comes off the wire in
 * its persistent form.
 *
 * A component with no case here is not refused. It is stored, persisted and reported by name, the
 * same bargain `/summon` strikes with the NBT keys it does not model.
 */
public object ItemComponentPayloads {
    /** Every component this can put on the wire. */
    public val supported: Set<String> get() = ENCODERS.keys

    /** [value] in its network form, or null when the value does not fit the component's shape. */
    public fun encode(
        name: String,
        value: NbtTag,
    ): ItemComponent? {
        val encoder = ENCODERS[name] ?: return null
        val id = componentId(name) ?: return null
        val buffer = ByteBuffer()
        if (!encoder(buffer, value)) return null
        return ItemComponent.Raw(id, buffer.toByteArray())
    }

    private fun componentId(name: String): Int? = org.basaltmc.server.registry.DataComponents[Identifier.parse(name)]

    /** A writer answers false when the value it was handed is not the shape the component takes. */
    private typealias Encoder = (ByteBuffer, NbtTag) -> Boolean

    private val DYE_COLOURS =
        listOf(
            "white",
            "orange",
            "magenta",
            "light_blue",
            "yellow",
            "lime",
            "pink",
            "gray",
            "light_gray",
            "cyan",
            "purple",
            "blue",
            "brown",
            "green",
            "red",
            "black",
        )

    private val SWING_ANIMATIONS = listOf("none", "whack", "stab")

    /** The components vanilla declares `persistent` and never `networkSynchronized`. */
    private val PERSISTENT_ONLY =
        listOf(
            "minecraft:intangible_projectile",
            "minecraft:map_decorations",
            "minecraft:debug_stick_state",
            "minecraft:recipes",
            "minecraft:lock",
            "minecraft:container_loot",
        )

    private val ENCODERS: Map<String, Encoder> =
        buildMap {
            // parity: `ByteBufCodecs.FLOAT`, `VAR_INT` and `Identifier.STREAM_CODEC`, one value each.
            put("minecraft:minimum_attack_charge", floatEncoder())
            put("minecraft:potion_duration_scale", floatEncoder())
            put("minecraft:additional_trade_cost", varIntEncoder())
            put("minecraft:map_id", varIntEncoder())
            put("minecraft:ominous_bottle_amplifier", varIntEncoder())
            put("minecraft:item_model", identifierEncoder())
            put("minecraft:tooltip_style", identifierEncoder())
            put("minecraft:note_block_sound", identifierEncoder())
            put("minecraft:glider") { _, _ -> true }

            // parity: BannerPatternLayers.STREAM_CODEC. Reference holders use registry id + 1;
            // zero is reserved for a direct, inline pattern.
            put("minecraft:banner_patterns") { buffer, value ->
                val layers = value as? NbtList ?: return@put false
                buffer.writeVarInt(layers.size)
                for (index in 0 until layers.size) {
                    val layer = layers[index] as? NbtCompound ?: return@put false
                    val pattern = layer.getString("pattern").substringAfter(':')
                    val patternId = VanillaRegistryData.bannerPatterns.indexOf(pattern)
                    val colorId = DYE_COLOURS.indexOf(layer.getString("color"))
                    if (patternId < 0 || colorId < 0) return@put false
                    buffer.writeVarInt(patternId + 1)
                    buffer.writeVarInt(colorId)
                }
                true
            }
            put("minecraft:tooltip_display") { buffer, value ->
                val display = value as? NbtCompound ?: return@put false
                buffer.writeBoolean(boolField(display, "hide_tooltip") ?: false)
                val hidden = display["hidden_components"] as? NbtList
                buffer.writeVarInt(hidden?.size ?: 0)
                for (index in 0 until (hidden?.size ?: 0)) {
                    val name = (hidden?.get(index) as? NbtString)?.value ?: return@put false
                    val component = componentId(name) ?: return@put false
                    buffer.writeVarInt(component)
                }
                true
            }

            // parity: the `idMapper` enums, written as their own ordinal.
            put("minecraft:dye", enumEncoder(DYE_COLOURS))
            put("minecraft:base_color", enumEncoder(DYE_COLOURS))
            put("minecraft:map_post_processing", enumEncoder(listOf("lock", "scale")))

            // parity: a component vanilla never gave a network codec falls back to
            // `ByteBufCodecs.fromCodecWithRegistries`, which puts its persistent value on the wire
            // as NBT. These seven are exactly those, so they need no shape of their own.
            for (name in PERSISTENT_ONLY) put(name, nbtEncoder())

            put("minecraft:enchantable") { buffer, value ->
                val level = intField(value, "value") ?: return@put false
                buffer.writeVarInt(level)
                true
            }
            // parity: `Weapon`, whose two fields default to 1 and 0.
            put("minecraft:weapon") { buffer, value ->
                val compound = value as? NbtCompound ?: return@put false
                buffer.writeVarInt(intField(compound, "item_damage_per_attack") ?: 1)
                buffer.writeFloat(floatField(compound, "disable_blocking_for_seconds") ?: 0f)
                true
            }
            // parity: `AttackRange`, six floats with vanilla's own defaults.
            put("minecraft:attack_range") { buffer, value ->
                val compound = value as? NbtCompound ?: return@put false
                buffer.writeFloat(floatField(compound, "min_reach") ?: 0f)
                buffer.writeFloat(floatField(compound, "max_reach") ?: DEFAULT_MAX_REACH)
                buffer.writeFloat(floatField(compound, "min_creative_reach") ?: 0f)
                buffer.writeFloat(floatField(compound, "max_creative_reach") ?: DEFAULT_CREATIVE_REACH)
                buffer.writeFloat(floatField(compound, "hitbox_margin") ?: 0f)
                buffer.writeFloat(floatField(compound, "mob_factor") ?: 1f)
                true
            }
            put("minecraft:food") { buffer, value ->
                val compound = value as? NbtCompound ?: return@put false
                buffer.writeVarInt(intField(compound, "nutrition") ?: return@put false)
                buffer.writeFloat(floatField(compound, "saturation") ?: return@put false)
                buffer.writeBoolean(boolField(compound, "can_always_eat") ?: false)
                true
            }
            put("minecraft:use_cooldown") { buffer, value ->
                val compound = value as? NbtCompound ?: return@put false
                buffer.writeFloat(floatField(compound, "seconds") ?: return@put false)
                val group = (compound["cooldown_group"] as? NbtString)?.value
                buffer.writeBoolean(group != null)
                group?.let { buffer.writeProtocolString(it, MAX_ID_LENGTH) }
                true
            }
            put("minecraft:swing_animation") { buffer, value ->
                val compound = value as? NbtCompound ?: return@put false
                val type = (compound["type"] as? NbtString)?.value ?: "whack"
                val index = SWING_ANIMATIONS.indexOf(type)
                if (index < 0) return@put false
                buffer.writeVarInt(index)
                buffer.writeVarInt(intField(compound, "duration") ?: DEFAULT_SWING_DURATION)
                true
            }
            put("minecraft:repairable") { buffer, value ->
                val items = (value as? NbtCompound)?.get("items") ?: return@put false
                writeHolderSet(buffer, items) { Items.REGISTRY.byIdentifier(it)?.protocolId }
            }
            // parity: `Tool`, a list of rules and three defaults.
            put("minecraft:tool") { buffer, value ->
                val compound = value as? NbtCompound ?: return@put false
                val rules = compound["rules"] as? NbtList ?: return@put false
                buffer.writeVarInt(rules.size)
                for (index in 0 until rules.size) {
                    val rule = rules[index] as? NbtCompound ?: return@put false
                    if (!writeHolderSet(buffer, rule["blocks"] ?: return@put false) {
                            Blocks.blockIdOf(it)
                        }
                    ) {
                        return@put false
                    }
                    val speed = floatField(rule, "speed")
                    buffer.writeBoolean(speed != null)
                    speed?.let(buffer::writeFloat)
                    val correct = boolField(rule, "correct_for_drops")
                    buffer.writeBoolean(correct != null)
                    correct?.let { buffer.writeBoolean(it) }
                }
                buffer.writeFloat(floatField(compound, "default_mining_speed") ?: 1f)
                buffer.writeVarInt(intField(compound, "damage_per_block") ?: 1)
                buffer.writeBoolean(boolField(compound, "can_destroy_blocks_in_creative") ?: true)
                true
            }
        }

    /**
     * parity: `ByteBufCodecs.holderSet`: a zero and a tag name, or the count plus one followed by
     * the plain registry ids.
     */
    private fun writeHolderSet(
        buffer: ByteBuffer,
        value: NbtTag,
        idOf: (Identifier) -> Int?,
    ): Boolean {
        if (value is NbtString) {
            val text = value.value
            if (text.startsWith("#")) {
                buffer.writeVarInt(0)
                buffer.writeProtocolString(text.removePrefix("#"), MAX_ID_LENGTH)
                return true
            }
            val id = idOf(Identifier.parse(text)) ?: return false
            buffer.writeVarInt(2)
            buffer.writeVarInt(id)
            return true
        }
        val list = value as? NbtList ?: return false
        val ids =
            (0 until list.size).map { index ->
                val name = (list[index] as? NbtString)?.value ?: return false
                idOf(Identifier.parse(name)) ?: return false
            }
        buffer.writeVarInt(ids.size + 1)
        for (id in ids) buffer.writeVarInt(id)
        return true
    }

    private fun floatEncoder(): Encoder =
        { buffer, value ->
            val number = numberOf(value)
            if (number == null) false else true.also { buffer.writeFloat(number) }
        }

    private fun varIntEncoder(): Encoder =
        { buffer, value ->
            val number = numberOf(value)
            if (number == null) false else true.also { buffer.writeVarInt(number.toInt()) }
        }

    private fun nbtEncoder(): Encoder =
        { buffer, value ->
            buffer.writeAnonymousNbt(value)
            true
        }

    private fun identifierEncoder(): Encoder =
        { buffer, value ->
            val text = (value as? NbtString)?.value
            if (text == null) false else true.also { buffer.writeProtocolString(text, MAX_ID_LENGTH) }
        }

    private fun enumEncoder(names: List<String>): Encoder =
        { buffer, value ->
            val index = names.indexOf((value as? NbtString)?.value)
            if (index < 0) false else true.also { buffer.writeVarInt(index) }
        }

    private fun intField(
        value: NbtTag,
        field: String,
    ): Int? = floatField(value, field)?.toInt()

    private fun floatField(
        value: NbtTag,
        field: String,
    ): Float? = numberOf((value as? NbtCompound)?.get(field) ?: return null)

    private fun boolField(
        value: NbtTag,
        field: String,
    ): Boolean? = numberOf((value as? NbtCompound)?.get(field) ?: return null)?.let { it != 0f }

    private fun numberOf(value: NbtTag?): Float? =
        when (value) {
            is NbtFloat -> value.value
            is NbtDouble -> value.value.toFloat()
            is NbtInt -> value.value.toFloat()
            is NbtByte -> value.value.toFloat()
            else -> null
        }

    private const val MAX_ID_LENGTH = 32767
    private const val DEFAULT_MAX_REACH = 3f
    private const val DEFAULT_CREATIVE_REACH = 5f
    private const val DEFAULT_SWING_DURATION = 6
}
