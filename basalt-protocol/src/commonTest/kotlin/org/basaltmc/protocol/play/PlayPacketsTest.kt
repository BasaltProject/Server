package org.basaltmc.protocol.play

import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.writeBoolean
import org.basaltmc.protocol.writeProtocolString
import org.basaltmc.protocol.writeVarInt
import org.basaltmc.text.Component
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

class PlayPacketsTest {
    @Test
    fun globalPosRoundTrips() {
        val buffer = ByteBuffer()
        val pos = GlobalPos("minecraft:overworld", BlockPos(1, 64, -2))
        pos.write(buffer)
        assertEquals(pos, GlobalPos.read(buffer))
    }

    @Test
    fun spawnInfoRoundTripsWithoutDeathLocation() {
        val buffer = ByteBuffer()
        val info = SpawnInfo(0, "minecraft:overworld", 42L, Gamemode.SURVIVAL, 255, false, true, null, 0, 63)
        info.write(buffer)
        assertEquals(info, SpawnInfo.read(buffer))
    }

    @Test
    fun spawnInfoRoundTripsWithADeathLocation() {
        val buffer = ByteBuffer()
        val death = GlobalPos("minecraft:overworld", BlockPos(0, 70, 0))
        val info = SpawnInfo(0, "minecraft:overworld", 42L, Gamemode.SURVIVAL, 0, false, false, death, 10, 63)
        info.write(buffer)
        assertEquals(info, SpawnInfo.read(buffer))
    }

    @Test
    fun loginPlayRoundTrips() {
        val buffer = ByteBuffer()
        val info = SpawnInfo(0, "minecraft:overworld", 42L, Gamemode.SURVIVAL, 255, false, true, null, 0, 63)
        val packet =
            LoginPlayPacket(1, false, listOf("minecraft:overworld"), 20, 10, 10, false, true, false, info, false)
        packet.write(buffer)
        assertEquals(packet, LoginPlayPacket.read(buffer))
    }

    @Test
    fun heightmapRoundTrips() {
        val buffer = ByteBuffer()
        val heightmap = Heightmap(HeightmapType.MOTION_BLOCKING, longArrayOf(1L, 2L, 3L))
        heightmap.write(buffer)
        assertEquals(heightmap, Heightmap.read(buffer))
    }

    @Test
    fun mapChunkRoundTripsAFullyPopulatedChunk() {
        val buffer = ByteBuffer()
        val blockValues = IntArray(4096) { if (it < 256) BEDROCK_STATE else AIR_STATE }
        val chunkData = buildSingleSectionChunkData(blockValues)
        val packet =
            MapChunkPacket(
                x = 3,
                z = -4,
                heightmaps = listOf(Heightmap(HeightmapType.MOTION_BLOCKING, longArrayOf(0L))),
                chunkData = chunkData,
                skyLightMask = longArrayOf(0b1L),
                blockLightMask = longArrayOf(0L),
                emptySkyLightMask = longArrayOf(0L),
                emptyBlockLightMask = longArrayOf(0b1L),
                skyLight = listOf(NibbleArray.uniform(15)),
                blockLight = emptyList(),
            )
        packet.write(buffer)
        val result = MapChunkPacket.read(buffer)
        assertEquals(packet.x, result.x)
        assertEquals(packet.z, result.z)
        assertEquals(packet.heightmaps, result.heightmaps)
        assertEquals(packet.chunkData.toList(), result.chunkData.toList())
        assertEquals(packet.skyLightMask.toList(), result.skyLightMask.toList())
        assertEquals(packet.emptyBlockLightMask.toList(), result.emptyBlockLightMask.toList())
        assertEquals(1, result.skyLight.size)
        assertEquals(packet.skyLight[0].toList(), result.skyLight[0].toList())
        assertEquals(0, result.blockLight.size)
    }

    @Test
    fun updateLightRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            UpdateLightPacket(
                chunkX = 1,
                chunkZ = 2,
                skyLightMask = longArrayOf(0b101L),
                blockLightMask = longArrayOf(0L),
                emptySkyLightMask = longArrayOf(0L),
                emptyBlockLightMask = longArrayOf(0b1L),
                skyLight = listOf(NibbleArray.uniform(15), NibbleArray.uniform(0)),
                blockLight = emptyList(),
            )
        packet.write(buffer)
        assertEquals(packet, UpdateLightPacket.read(buffer))
    }

    @Test
    fun playerPositionRoundTrips() {
        val buffer = ByteBuffer()
        val packet = PlayerPositionPacket(1, 8.5, 5.0, 8.5, 0.0, 0.0, 0.0, 90f, 0f, 0)
        packet.write(buffer)
        assertEquals(packet, PlayerPositionPacket.read(buffer))
    }

    @Test
    fun acceptTeleportationRoundTrips() {
        val buffer = ByteBuffer()
        val packet = AcceptTeleportationPacket(7)
        packet.write(buffer)
        assertEquals(packet, AcceptTeleportationPacket.read(buffer))
    }

    @Test
    fun spawnPositionRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SpawnPositionPacket(GlobalPos("minecraft:overworld", BlockPos(0, 4, 0)), 0f, 0f)
        packet.write(buffer)
        assertEquals(packet, SpawnPositionPacket.read(buffer))
    }

    @OptIn(ExperimentalUuidApi::class)
    @Test
    fun playerInfoAddRoundTrips() {
        val buffer = ByteBuffer()
        val packet =
            PlayerInfoAddPacket(listOf(PlayerInfoEntry(Uuid.random(), "Steve", listed = true, latencyMillis = 42)))
        packet.write(buffer)
        assertEquals(packet, PlayerInfoAddPacket.read(buffer))
    }

    @OptIn(ExperimentalUuidApi::class)
    @Test
    fun playerInfoRemoveRoundTrips() {
        val buffer = ByteBuffer()
        val packet = PlayerInfoRemovePacket(listOf(Uuid.random(), Uuid.random()))
        packet.write(buffer)
        assertEquals(packet, PlayerInfoRemovePacket.read(buffer))
    }

    @Test
    fun entityTeleportRoundTrips() {
        val buffer = ByteBuffer()
        val packet = EntityTeleportPacket(5, 1.0, 2.0, 3.0, (-64).toByte(), 32, true)
        packet.write(buffer)
        assertEquals(packet, EntityTeleportPacket.read(buffer))
    }

    @Test
    fun playKeepAliveRoundTrips() {
        val buffer = ByteBuffer()
        val packet = PlayKeepAlivePacket(9999999999L)
        packet.write(buffer)
        assertEquals(packet, PlayKeepAlivePacket.read(buffer))
    }

    @Test
    fun systemChatRoundTrips() {
        val buffer = ByteBuffer()
        val packet = SystemChatPacket(Component.text("hello"), isActionBar = false)
        packet.write(buffer)
        assertEquals(packet, SystemChatPacket.read(buffer))
    }

    @Test
    fun playDisconnectRoundTrips() {
        val buffer = ByteBuffer()
        val packet = PlayDisconnectPacket(Component.text("kicked"))
        packet.write(buffer)
        assertEquals(packet, PlayDisconnectPacket.read(buffer))
    }

    @Test
    fun chatMessageRoundTripsWithoutASignature() {
        val buffer = ByteBuffer()
        val packet = ChatMessagePacket("hi", 1L, 2L, null, 0, byteArrayOf(0, 0, 0), 0.toByte())
        writeChatMessage(buffer, packet)
        val result = ChatMessagePacket.read(buffer)
        assertEquals(packet.message, result.message)
        assertNull(result.signature)
    }

    @Test
    fun chatMessageRoundTripsWithASignature() {
        val buffer = ByteBuffer()
        val signature = ByteArray(256) { it.toByte() }
        val packet = ChatMessagePacket("signed message", 1L, 2L, signature, 3, byteArrayOf(1, 2, 3), 9.toByte())
        writeChatMessage(buffer, packet)
        val result = ChatMessagePacket.read(buffer)
        assertEquals(packet, result)
    }

    @Test
    fun serverboundPositionRoundTrips() {
        val buffer = ByteBuffer()
        buffer.writeDouble(1.0)
        buffer.writeDouble(2.0)
        buffer.writeDouble(3.0)
        buffer.writeByte(1.toByte())
        val result = ServerboundPositionPacket.read(buffer)
        assertEquals(ServerboundPositionPacket(1.0, 2.0, 3.0, onGround = true), result)
    }

    @Test
    fun serverboundPositionLookRoundTrips() {
        val buffer = ByteBuffer()
        buffer.writeDouble(1.0)
        buffer.writeDouble(2.0)
        buffer.writeDouble(3.0)
        buffer.writeFloat(90f)
        buffer.writeFloat(0f)
        buffer.writeByte(0.toByte())
        val result = ServerboundPositionLookPacket.read(buffer)
        assertEquals(ServerboundPositionLookPacket(1.0, 2.0, 3.0, 90f, 0f, onGround = false), result)
    }
}

private const val AIR_STATE = 0
private const val BEDROCK_STATE = 85

private fun buildSingleSectionChunkData(blockValues: IntArray): ByteArray {
    val buffer = ByteBuffer()
    buffer.writeShort(256.toShort())
    PalettedContainer.write(
        buffer,
        blockValues,
        PalettedContainer.BLOCK_MIN_INDIRECT_BITS,
        PalettedContainer.BLOCK_MAX_INDIRECT_BITS,
        8,
    )
    PalettedContainer.write(
        buffer,
        IntArray(64),
        PalettedContainer.BIOME_MIN_INDIRECT_BITS,
        PalettedContainer.BIOME_MAX_INDIRECT_BITS,
        6,
    )
    return buffer.toByteArray()
}

private fun writeChatMessage(
    buffer: ByteBuffer,
    packet: ChatMessagePacket,
) {
    buffer.writeProtocolString(packet.message, 256)
    buffer.writeLong(packet.timestamp)
    buffer.writeLong(packet.salt)
    buffer.writeBoolean(packet.signature != null)
    packet.signature?.let { buffer.writeBytes(it) }
    buffer.writeVarInt(packet.offset)
    buffer.writeBytes(packet.acknowledged)
    buffer.writeByte(packet.checksum)
}
