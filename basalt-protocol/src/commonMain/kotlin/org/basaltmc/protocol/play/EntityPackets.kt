package org.basaltmc.protocol.play

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.ProtocolException
import org.basaltmc.protocol.readUuid
import org.basaltmc.protocol.readVarInt
import org.basaltmc.protocol.writeUuid
import org.basaltmc.protocol.writeVarInt
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.round
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * Vanilla's "low precision" `Vec3` wire encoding, used for entity velocity. Mirrors `LpVec3`: the
 * zero vector is a single zero byte, anything else shares one common power-of-four scale across
 * all three axes, quantized to 15 bits each. Verified against the real 26.2 server jar's decompiled
 * `LpVec3`, not the flat `* 8000` short encoding older protocol versions used here.
 */
public object LpVec3 {
    private const val DATA_BITS_MASK = 32767L
    private const val MAX_QUANTIZED_VALUE = 32766.0
    private const val CONTINUATION_FLAG = 4L
    private const val ABS_MAX_VALUE = 1.7179869183E10
    private const val ABS_MIN_VALUE = 3.051944088384301E-5

    public fun write(
        buffer: ByteBuffer,
        x: Double,
        y: Double,
        z: Double,
    ) {
        val sx = sanitize(x)
        val sy = sanitize(y)
        val sz = sanitize(z)
        val chessboard = maxOf(abs(sx), abs(sy), abs(sz))
        if (chessboard < ABS_MIN_VALUE) {
            buffer.writeByte(0)
            return
        }
        val scale = ceil(chessboard).toLong()
        val isPartial = (scale and 3L) != scale
        val markers = if (isPartial) (scale and 3L) or CONTINUATION_FLAG else scale
        val packed =
            markers or
                (pack(sx / scale) shl 3) or
                (pack(sy / scale) shl 18) or
                (pack(sz / scale) shl 33)
        buffer.writeByte(packed.toByte())
        buffer.writeByte((packed shr 8).toByte())
        buffer.writeInt((packed shr 16).toInt())
        if (isPartial) buffer.writeVarInt((scale shr 2).toInt())
    }

    public fun read(buffer: ByteBuffer): Triple<Double, Double, Double> {
        val lowest = buffer.readByte().toInt() and 0xFF
        if (lowest == 0) return Triple(0.0, 0.0, 0.0)
        val middle = buffer.readByte().toInt() and 0xFF
        val highest = buffer.readInt().toLong() and 0xFFFFFFFFL
        val packed = (highest shl 16) or (middle.toLong() shl 8) or lowest.toLong()
        var scale = lowest.toLong() and 3L
        if ((lowest.toLong() and CONTINUATION_FLAG) == CONTINUATION_FLAG) {
            scale = scale or ((buffer.readVarInt().toLong() and 0xFFFFFFFFL) shl 2)
        }
        return Triple(
            unpack(packed shr 3) * scale,
            unpack(packed shr 18) * scale,
            unpack(packed shr 33) * scale,
        )
    }

    private fun sanitize(value: Double): Double =
        if (value.isNaN()) 0.0 else value.coerceIn(-ABS_MAX_VALUE, ABS_MAX_VALUE)

    private fun pack(value: Double): Long = round((value * 0.5 + 0.5) * MAX_QUANTIZED_VALUE).toLong()

    private fun unpack(value: Long): Double =
        minOf(value and DATA_BITS_MASK, MAX_QUANTIZED_VALUE.toLong()).toDouble() * 2.0 / MAX_QUANTIZED_VALUE - 1.0
}

/** `Mth.packDegrees`/`unpackDegrees`: a rotation as a single byte, 256 steps per full turn. */
private fun packDegrees(value: Float): Byte = floor(value * 256f / 360f).toInt().toByte()

private fun unpackDegrees(value: Byte): Float = value * 360f / 256f

/**
 * Clientbound, id 0x01: spawns a non-living entity (this build only ever spawns dropped items).
 * Mirrors `ClientboundAddEntityPacket`. [entityType] is the numeric protocol id of the entity's
 * type (verified against the real 26.2 registries; `minecraft:item` is 71, the only one this build
 * sends). [objectData] carries type-specific extra data (a thrown potion's item id, a falling
 * block's block state...); unused for items, always 0 here.
 */
@OptIn(ExperimentalUuidApi::class)
public data class SpawnEntityPacket(
    val entityId: Int,
    val uuid: Uuid,
    val entityType: Int,
    val x: Double,
    val y: Double,
    val z: Double,
    val velocityX: Double = 0.0,
    val velocityY: Double = 0.0,
    val velocityZ: Double = 0.0,
    val pitch: Float = 0f,
    val yaw: Float = 0f,
    val headYaw: Float = 0f,
    val objectData: Int = 0,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        buffer.writeUuid(uuid)
        buffer.writeVarInt(entityType)
        buffer.writeDouble(x)
        buffer.writeDouble(y)
        buffer.writeDouble(z)
        LpVec3.write(buffer, velocityX, velocityY, velocityZ)
        buffer.writeByte(packDegrees(pitch))
        buffer.writeByte(packDegrees(yaw))
        buffer.writeByte(packDegrees(headYaw))
        buffer.writeVarInt(objectData)
    }

    public companion object {
        public const val ID: Int = 0x01

        /** Protocol id of `minecraft:item` in the real 26.2 entity type registry. */
        public const val ITEM_ENTITY_TYPE: Int = 71

        public fun read(buffer: ByteBuffer): SpawnEntityPacket {
            val entityId = buffer.readVarInt()
            val uuid = buffer.readUuid()
            val entityType = buffer.readVarInt()
            val x = buffer.readDouble()
            val y = buffer.readDouble()
            val z = buffer.readDouble()
            val (vx, vy, vz) = LpVec3.read(buffer)
            val pitch = unpackDegrees(buffer.readByte())
            val yaw = unpackDegrees(buffer.readByte())
            val headYaw = unpackDegrees(buffer.readByte())
            val objectData = buffer.readVarInt()
            return SpawnEntityPacket(entityId, uuid, entityType, x, y, z, vx, vy, vz, pitch, yaw, headYaw, objectData)
        }
    }
}

/**
 * Clientbound, id 0x63: entity metadata. Mirrors `ClientboundSetEntityDataPacket`, but only ever
 * carries the one entry this build needs: a dropped item's stack, `ItemEntity.DATA_ITEM` (index 8,
 * the "item stack" serializer, id 7; `Entity` itself claims indices 0-7, verified against the real
 * 26.2 server jar's `EntityDataSerializers`/`ItemEntity`). A generic metadata entry list is not
 * implemented, since dropped items are the only metadata-bearing entity this build spawns; reading
 * anything else throws, the same boundary [WireItemStack] draws for item components.
 */
public data class SetEntityDataPacket(
    val entityId: Int,
    val item: WireItemStack?,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(entityId)
        buffer.writeByte(ITEM_STACK_INDEX.toByte())
        buffer.writeVarInt(ITEM_STACK_SERIALIZER_ID)
        WireItemStack.write(buffer, item)
        buffer.writeByte(END_OF_METADATA.toByte())
    }

    public companion object {
        public const val ID: Int = 0x63
        private const val ITEM_STACK_INDEX = 8
        private const val ITEM_STACK_SERIALIZER_ID = 7
        private const val END_OF_METADATA = 0xFF

        public fun read(buffer: ByteBuffer): SetEntityDataPacket {
            val entityId = buffer.readVarInt()
            val index = buffer.readByte().toInt() and 0xFF
            if (index != ITEM_STACK_INDEX) {
                throw ProtocolException("Only the item-stack metadata entry is supported, got index $index")
            }
            val serializerId = buffer.readVarInt()
            if (serializerId != ITEM_STACK_SERIALIZER_ID) {
                throw ProtocolException("Only the item-stack serializer is supported, got id $serializerId")
            }
            val item = WireItemStack.read(buffer)
            val terminator = buffer.readByte().toInt() and 0xFF
            if (terminator != END_OF_METADATA) {
                throw ProtocolException("Expected a single metadata entry, found more after index $index")
            }
            return SetEntityDataPacket(entityId, item)
        }
    }
}

/**
 * Clientbound, id 0x7c: plays the "item flies to the player" pickup animation. Mirrors
 * `ClientboundTakeItemEntityPacket`. Purely cosmetic: the inventory change itself travels through
 * the normal container packets, sent alongside this one.
 */
public data class ClientboundTakeItemEntityPacket(
    val collectedEntityId: Int,
    val collectorEntityId: Int,
    val pickupItemCount: Int,
) {
    public fun write(buffer: ByteBuffer) {
        buffer.writeVarInt(collectedEntityId)
        buffer.writeVarInt(collectorEntityId)
        buffer.writeVarInt(pickupItemCount)
    }

    public companion object {
        public const val ID: Int = 0x7c

        public fun read(buffer: ByteBuffer): ClientboundTakeItemEntityPacket =
            ClientboundTakeItemEntityPacket(buffer.readVarInt(), buffer.readVarInt(), buffer.readVarInt())
    }
}
