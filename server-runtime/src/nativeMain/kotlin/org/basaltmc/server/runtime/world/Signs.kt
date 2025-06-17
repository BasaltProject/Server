package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.NbtType

/**
 * What a sign holds and how it is written down.
 *
 * parity: `SignBlockEntity` over `SignText`: two sides, four lines each, a dye colour and whether
 * the text glows. A line is a text component, and a plain one is written as a bare NBT string,
 * which is the short form the client reads.
 */
public object Signs {
    public const val LINES: Int = 4

    /** parity: `SignBlockEntity.MAX_TEXT_LINE_LENGTH`, which is what the server enforces. */
    public const val MAX_LINE_LENGTH: Int = 384

    private const val FRONT = "front_text"
    private const val BACK = "back_text"

    public fun isSign(stateId: Int): Boolean {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return false
        return path.endsWith("_sign") || path.endsWith("_hanging_sign")
    }

    /** The block entity type a sign of this block is: the hanging ones are their own. */
    public fun typeOf(stateId: Int): Identifier? {
        val path = BlockStateDefinitions.blockOf(stateId)?.path ?: return null
        return when {
            path.endsWith("_hanging_sign") -> Identifier.vanilla("hanging_sign")
            path.endsWith("_sign") -> Identifier.vanilla("sign")
            else -> null
        }
    }

    /** An empty sign, which is what a freshly placed one holds until someone writes on it. */
    public fun emptyData(): NbtCompound =
        NbtCompound().apply {
            this[FRONT] = side(List(LINES) { "" })
            this[BACK] = side(List(LINES) { "" })
            putByte("is_waxed", 0)
        }

    /** The same with one side replaced by [lines], each trimmed to what a sign accepts. */
    public fun withText(
        data: NbtCompound,
        isFront: Boolean,
        lines: List<String>,
    ): NbtCompound {
        val updated = NbtCompound()
        for (key in data.keys) data[key]?.let { updated[key] = it }
        updated[if (isFront) FRONT else BACK] = side(lines.map { it.take(MAX_LINE_LENGTH) })
        return updated
    }

    /** Whether this sign refuses to be edited. parity: `SignBlockEntity.isWaxed`. */
    public fun isWaxed(data: NbtCompound): Boolean = data.getByte("is_waxed").toInt() != 0

    public fun blockEntity(
        localX: Int,
        y: Int,
        localZ: Int,
        type: Identifier,
        data: NbtCompound = emptyData(),
    ): ChunkBlockEntity = ChunkBlockEntity(localX, y, localZ, type, data)

    /** parity: `SignText`'s codec: the four messages, a colour and the glow flag, always present. */
    private fun side(lines: List<String>): NbtCompound =
        NbtCompound().apply {
            this["messages"] = NbtList(NbtType.STRING, lines.map { NbtString(it) as NbtTag })
            putString("color", "black")
            putByte("has_glowing_text", 0)
        }
}
