package org.basaltmc.protocol.play

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.ProtocolException
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeVarInt

/**
 * One stack on the wire: how many, of what, plus whatever data components it carries.
 *
 * Components are the open-ended part (enchantments, a custom name, damage) and each one has a codec
 * of its own keyed by registry id. This build **writes** stacks with none and **refuses to read**
 * one that has any, rather than pretending: a component cannot be skipped without knowing its
 * length, so a reader that ignored them would not know where the next stack begins and would
 * desynchronise the whole stream. That is the boundary of the inventory support in this build, and
 * it is a boundary that reports itself.
 */
public data class WireItemStack(
    val itemId: Int,
    val count: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(count)
        if (count <= 0) return
        buffer.writeVarInt(itemId)
        // The component patch: nothing added, nothing removed.
        buffer.writeVarInt(0)
        buffer.writeVarInt(0)
    }

    public companion object {
        /** A count of zero is how vanilla writes "this slot is empty"; the rest of the stack follows only if non-empty. */
        public val EMPTY: WireItemStack = WireItemStack(0, 0)

        public fun write(
            buffer: ByteBuffer,
            stack: WireItemStack?,
        ) {
            (stack ?: EMPTY).write(buffer)
        }

        public fun read(buffer: ByteBuffer): WireItemStack? {
            val count = buffer.readVarInt()
            if (count <= 0) return null
            val itemId = buffer.readVarInt()
            val added = buffer.readVarInt()
            val removed = buffer.readVarInt()
            if (added != 0 || removed != 0) {
                throw ProtocolException(
                    "Item stacks with data components are not supported yet " +
                        "($added added, $removed removed on item $itemId)",
                )
            }
            return WireItemStack(itemId, count)
        }

        /**
         * The same stack, in the "delimited" form `set_creative_slot` uses
         * (`ItemStack.OPTIONAL_UNTRUSTED_STREAM_CODEC` over
         * `DataComponentPatch.DELIMITED_STREAM_CODEC`, verified against the real 26.2 jar).
         *
         * Every component there carries its own varint length prefix, and that prefix is the whole
         * difference: [read] has to refuse a stack carrying components because a plain patch cannot
         * be walked without each component's own codec, but this form can be walked and skipped
         * blindly. Refusing here instead cost the player their connection the moment they dragged
         * anything with components (a damaged tool, a written book, a shulker box) out of the
         * creative menu, since an unhandled decode failure drops the session.
         *
         * The components themselves are still discarded, which is the same documented gap [read]
         * describes: this build models a stack as an item plus a count and nothing else.
         */
        public fun readDelimited(buffer: ByteBuffer): WireItemStack? {
            val count = buffer.readVarInt()
            if (count <= 0) return null
            val itemId = buffer.readVarInt()
            val added = buffer.readVarInt()
            val removed = buffer.readVarInt()
            repeat(added) {
                buffer.readVarInt() // component type id
                buffer.readBytes(buffer.readVarInt()) // its length-prefixed payload
            }
            // Removals are just the type ids, with no payload to skip.
            repeat(removed) { buffer.readVarInt() }
            return WireItemStack(itemId, count)
        }
    }
}

/**
 * Every slot of an open container at once, vanilla's `ClientboundContainerSetContentPacket`.
 *
 * Container 0 is the player's own inventory, which is the only one this build has.
 *
 * [stateId] is vanilla's optimistic-locking counter: the client echoes it back with every click so
 * the server can tell a click made against the current contents from one made against contents it
 * has since replaced. Sending a fresh one is what makes the client drop whatever it was predicting.
 */
public data class ContainerSetContentPacket(
    val containerId: Int,
    val stateId: Int,
    val items: List<WireItemStack?>,
    val carriedItem: WireItemStack? = null,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
        buffer.writeVarInt(stateId)
        buffer.writeVarInt(items.size)
        for (item in items) WireItemStack.write(buffer, item)
        WireItemStack.write(buffer, carriedItem)
    }

    public companion object {
        public const val ID: Int = 0x12
        public const val PLAYER_INVENTORY: Int = 0

        public fun read(buffer: ByteBuffer): ContainerSetContentPacket {
            val containerId = buffer.readVarInt()
            val stateId = buffer.readVarInt()
            val size = buffer.readVarInt()
            val items = (0 until size).map { WireItemStack.read(buffer) }
            return ContainerSetContentPacket(containerId, stateId, items, WireItemStack.read(buffer))
        }
    }
}

/** One slot of one container, for when a single thing changed. */
public data class ContainerSetSlotPacket(
    val containerId: Int,
    val stateId: Int,
    val slot: Int,
    val item: WireItemStack?,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(containerId)
        buffer.writeVarInt(stateId)
        buffer.writeShort(slot.toShort())
        WireItemStack.write(buffer, item)
    }

    public companion object {
        public const val ID: Int = 0x14

        public fun read(buffer: ByteBuffer): ContainerSetSlotPacket =
            ContainerSetSlotPacket(
                containerId = buffer.readVarInt(),
                stateId = buffer.readVarInt(),
                slot = buffer.readShort().toInt(),
                item = WireItemStack.read(buffer),
            )
    }
}

/** Which hotbar slot the player is holding, server to client (`ClientboundSetHeldSlotPacket`). */
public data class SetHeldSlotPacket(
    val slot: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(slot)
    }

    public companion object {
        public const val ID: Int = 0x69

        public fun read(buffer: ByteBuffer): SetHeldSlotPacket = SetHeldSlotPacket(buffer.readVarInt())
    }
}

/** The same, client to server: the player turned the mouse wheel or pressed a number key. */
public data class ServerboundSetCarriedItemPacket(
    val slot: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeShort(slot.toShort())
    }

    public companion object {
        public const val ID: Int = 0x35

        /** Hotbar slots only; anything else is a client sending nonsense. */
        public const val HOTBAR_SIZE: Int = 9

        public fun read(buffer: ByteBuffer): ServerboundSetCarriedItemPacket =
            ServerboundSetCarriedItemPacket(buffer.readShort().toInt())
    }
}

/**
 * Serverbound, id 0x38: creative-mode inventory editing. Mirrors
 * `ServerboundSetCreativeModeSlotPacket`. This is the *only* way a creative client's dragged item
 * (from the creative menu into a slot, or a slot into the void to discard it, which arrives as
 * [slot] `-1`) reaches the server: creative item movement never goes through a window-click packet.
 */
public data class ServerboundSetCreativeModeSlotPacket(
    val slot: Int,
    val item: WireItemStack?,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeShort(slot.toShort())
        WireItemStack.write(buffer, item)
    }

    public companion object {
        public const val ID: Int = 0x38

        public fun read(buffer: ByteBuffer): ServerboundSetCreativeModeSlotPacket =
            ServerboundSetCreativeModeSlotPacket(buffer.readShort().toInt(), WireItemStack.readDelimited(buffer))
    }
}
