package org.basaltmc.server.runtime

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `LivingEntity.DATA_LIVING_ENTITY_FLAGS`, the byte a client reads to know it is still using. */
class UsingItemFlagTest {
    @Test
    fun theUsingItemFlagRidesOnTheLivingEntityByte() {
        val buffer = ByteBuffer()
        SetEntityDataPacket(
            7,
            listOf(SetEntityDataPacket.Entry.Bytes(SetEntityDataPacket.LIVING_FLAGS_INDEX, 0x01)),
        ).write(buffer)

        val read = SetEntityDataPacket.read(buffer)
        val entry = read.entries.single() as SetEntityDataPacket.Entry.Bytes
        assertEquals(8, entry.index, "the first entry a living entity adds comes after the eight of Entity")
        assertEquals(0x01, entry.value.toInt())
    }
}
