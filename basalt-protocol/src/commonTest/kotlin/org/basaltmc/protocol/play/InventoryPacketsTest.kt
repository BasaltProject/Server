package org.basaltmc.protocol.play

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.ProtocolException
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

class InventoryPacketsTest {
    @Test
    fun aStackWithNoComponentsSurvivesARoundTrip() {
        val buffer = ByteBuffer()
        WireItemStack(itemId = 1, count = 64).write(buffer)

        assertEquals(WireItemStack(1, 64), WireItemStack.read(buffer))
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun anEmptySlotIsACountOfZeroAndNothingElse() {
        val buffer = ByteBuffer()
        WireItemStack.write(buffer, null)

        // One byte: writing the item id of an empty slot would be a byte the client does not read.
        assertEquals(1, buffer.readableBytes)
        assertNull(WireItemStack.read(buffer))
    }

    @Test
    fun aStackCarryingComponentsIsRefusedRatherThanMisread() {
        val buffer = ByteBuffer()
        // Hand-written: one component added, which this build has no codec for.
        buffer.writeByte(1)
        buffer.writeByte(1)
        buffer.writeByte(1)
        buffer.writeByte(0)

        // Refused, because a component cannot be skipped without knowing its length: a reader that
        // ignored it would not know where the next stack starts.
        assertFailsWith<ProtocolException> { WireItemStack.read(buffer) }
    }

    @Test
    fun theWholeWindowSurvivesARoundTrip() {
        val packet =
            ContainerSetContentPacket(
                containerId = ContainerSetContentPacket.PLAYER_INVENTORY,
                stateId = 7,
                items = listOf(WireItemStack(1, 1), null, WireItemStack(2, 32)),
                carriedItem = null,
            )
        val buffer = ByteBuffer()
        packet.write(buffer)

        assertEquals(packet, ContainerSetContentPacket.read(buffer))
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun oneSlotSurvivesARoundTrip() {
        val packet = ContainerSetSlotPacket(0, 3, 36, WireItemStack(5, 12))
        val buffer = ByteBuffer()
        packet.write(buffer)

        assertEquals(packet, ContainerSetSlotPacket.read(buffer))
        assertEquals(0, buffer.readableBytes)
    }

    @Test
    fun theHeldSlotSurvivesARoundTripInBothDirections() {
        val outgoing = ByteBuffer()
        SetHeldSlotPacket(4).write(outgoing)
        assertEquals(SetHeldSlotPacket(4), SetHeldSlotPacket.read(outgoing))

        val incoming = ByteBuffer()
        ServerboundSetCarriedItemPacket(8).write(incoming)
        assertEquals(ServerboundSetCarriedItemPacket(8), ServerboundSetCarriedItemPacket.read(incoming))
    }
}
