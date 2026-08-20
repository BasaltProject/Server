package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** parity: `Entity.saveWithoutId`, in the keys it shares with it. */
@OptIn(ExperimentalUuidApi::class)
class EntityNbtTest {
    private fun zombie(): ServerEntity =
        ServerEntity(
            entityId = 7,
            uuid = Uuid.parse("11111111-2222-3333-4444-555555555555"),
            type = Identifier.vanilla("zombie"),
            protocolType = 151,
            x = 8.5,
            y = 64.0,
            z = -3.25,
            yaw = 90f,
            pitch = -12f,
        )

    @Test
    fun anEntityComesBackAsItWent() {
        val zombie = zombie()
        zombie.health = 13.5
        zombie.age = 42
        zombie.velocityX = 0.25
        zombie.velocityZ = -0.5
        zombie.persistent = true
        zombie.customName = "Bob"
        zombie.tags += "friendly"

        val restored = assertNotNull(EntityNbt.decode(EntityNbt.encode(zombie), entityId = 9, protocolType = 151))
        assertEquals(9, restored.entityId)
        assertEquals(zombie.uuid, restored.uuid)
        assertEquals(zombie.type, restored.type)
        assertEquals(8.5, restored.x)
        assertEquals(-3.25, restored.z)
        assertEquals(90f, restored.yaw)
        assertEquals(13.5, restored.health)
        assertEquals(42, restored.age)
        assertEquals(0.25, restored.velocityX)
        assertTrue(restored.persistent)
        assertEquals("Bob", restored.customName)
        assertTrue("friendly" in restored.tags)
    }

    @Test
    fun aVillagerKeepsItsTrade() {
        val villager = zombie()
        villager.profession = "librarian"
        villager.villagerLevel = 3
        villager.villagerExperience = 55

        val restored = assertNotNull(EntityNbt.decode(EntityNbt.encode(villager), entityId = 1, protocolType = 1))
        assertEquals("librarian", restored.profession)
        assertEquals(3, restored.villagerLevel)
        assertEquals(55, restored.villagerExperience)
    }

    @Test
    fun aTagWithoutAPositionIsNoEntity() {
        val tag = EntityNbt.encode(zombie())
        tag.remove("Pos")
        assertNull(EntityNbt.decode(tag, entityId = 1, protocolType = 1))
    }
}
