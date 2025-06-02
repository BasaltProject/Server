package org.basaltmc.protocol.play

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.writeVarInt
import kotlin.math.abs
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

@OptIn(ExperimentalUuidApi::class)
class EntityPacketsTest {
    @Test
    fun lpVec3RoundTripsZero() {
        val buffer = ByteBuffer()
        LpVec3.write(buffer, 0.0, 0.0, 0.0)
        assertEquals(Triple(0.0, 0.0, 0.0), LpVec3.read(buffer))
    }

    @Test
    fun lpVec3RoundTripsWithinQuantizationError() {
        val buffer = ByteBuffer()
        LpVec3.write(buffer, 0.2, -0.4, 1.5)
        val (x, y, z) = LpVec3.read(buffer)
        assertTrue(abs(0.2 - x) < 0.001)
        assertTrue(abs(-0.4 - y) < 0.001)
        assertTrue(abs(1.5 - z) < 0.001)
    }

    @Test
    fun spawnEntityRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            SpawnEntityPacket(
                entityId = 42,
                uuid = Uuid.parse("11111111-2222-3333-4444-555555555555"),
                entityType = SpawnEntityPacket.ITEM_ENTITY_TYPE,
                x = 10.5,
                y = 64.0,
                z = -10.5,
            )
        packet.write(buffer)
        assertEquals(packet, SpawnEntityPacket.read(buffer))
    }

    @Test
    fun setEntityDataRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SetEntityDataPacket(entityId = 7, item = WireItemStack(itemId = 5, count = 3))
        packet.write(buffer)
        assertEquals(packet, SetEntityDataPacket.read(buffer))
    }

    @Test
    fun setEntityDataRoundTripsEmptyStack() {
        val buffer = ByteBuffer()
        val packet = SetEntityDataPacket(entityId = 7, item = null)
        packet.write(buffer)
        assertEquals(packet, SetEntityDataPacket.read(buffer))
    }

    @Test
    fun clientboundTakeItemEntityRoundTrips() {
        val buffer = ByteBuffer()
        val packet = ClientboundTakeItemEntityPacket(collectedEntityId = 9, collectorEntityId = 1, pickupItemCount = 16)
        packet.write(buffer)
        assertEquals(packet, ClientboundTakeItemEntityPacket.read(buffer))
    }

    @Test
    fun serverboundPickItemFromBlockRoundTrips() {
        val buffer = ByteBuffer()
        val packet = ServerboundPickItemFromBlockPacket(BlockPos(1, 64, -1), includeData = true)
        packet.write(buffer)
        assertEquals(packet, ServerboundPickItemFromBlockPacket.read(buffer))
    }

    @Test
    fun setEntityDataRejectsAnUnknownIndex() {
        val buffer = ByteBuffer()
        buffer.writeVarInt(1)
        buffer.writeByte(0)
        buffer.writeVarInt(0)
        buffer.writeByte(1)
        buffer.writeByte(0xFF.toByte())
        assertFailsWith<org.basaltmc.protocol.ProtocolException> { SetEntityDataPacket.read(buffer) }
    }
}
