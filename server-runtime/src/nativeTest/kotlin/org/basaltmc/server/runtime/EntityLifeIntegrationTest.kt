package org.basaltmc.server.runtime

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.io.TcpConnection
import org.basaltmc.wire.protocol.play.ChatCommandPacket
import org.basaltmc.wire.protocol.play.ChunkBatchFinishedPacket
import org.basaltmc.wire.protocol.play.ChunkBatchReceivedPacket
import org.basaltmc.wire.protocol.play.EntityPositionSyncPacket
import org.basaltmc.wire.protocol.play.SpawnEntityPacket
import kotlin.test.Test
import kotlin.test.assertTrue

/**
 * A whole server, a real socket, and the entities living in it: what a client is actually told about
 * a mob that was summoned near it, and what happens to a block of TNT that is lit.
 *
 * The point is the wiring rather than any one number: the unit tests already cover the physics, and
 * this is what catches a tick loop that never runs them.
 */
class EntityLifeIntegrationTest {
    private var port = 0

    @Test
    fun aSummonedMobIsSpawnedForTheClientAndMovesOnItsOwn() {
        port = startTestServer(18620, defaultPermissions = listOf("basalt.command.summon", "basalt.command.help"))
        val connection = TcpConnection.connect("127.0.0.1", port)
        val incoming = ByteBuffer()
        val session = loginToPlay(connection, incoming, "Robin")

        // The spawn-radius chunks stream unprompted; a client that never reads them fills the
        // socket buffer and stalls the server's next write. See BlockInteractionIntegrationTest.
        val batch = ChunkBatchFinishedPacket.read(drainUntil(connection, incoming, ChunkBatchFinishedPacket.ID))
        assertTrue(batch.batchSize > 0)
        sendCompressed(connection, ChunkBatchReceivedPacket.ID, session.threshold) { it.writeFloat(16f) }

        // Five blocks up, so the first thing it does is fall: that is the tick loop, the physics
        // and the position sync all at once, and it happens within a few ticks rather than a minute.
        sendCompressed(connection, ChatCommandPacket.ID, session.threshold) {
            ChatCommandPacket("summon minecraft:cow ~ ~5 ~2").write(it)
        }

        val spawn = SpawnEntityPacket.read(drainUntil(connection, incoming, SpawnEntityPacket.ID))
        assertTrue(spawn.entityId != session.entityId, "the cow is not the player")

        var lowest = spawn.y
        repeat(20) {
            val sync = EntityPositionSyncPacket.read(drainUntil(connection, incoming, EntityPositionSyncPacket.ID))
            if (sync.entityId == spawn.entityId) lowest = minOf(lowest, sync.y)
        }
        assertTrue(lowest < spawn.y - 0.5, "the cow never fell: spawned at ${'$'}{spawn.y}, lowest ${'$'}lowest")

        connection.close()
    }
}
