package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.entity.EntityNbt
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.entity.ai.MobGoals
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** Durable villager Brain-memory regressions for social iron-golem summoning. */
@OptIn(ExperimentalUuidApi::class)
class VillageGolemSpawnerTest {
    @Test
    fun mobSummonedIronGolemMaterializesWithItsAi() {
        val forest =
            writeGeneratedForest(
                data = NbtCompound(),
                type = Identifier.vanilla("iron_golem"),
                groupIndex = 0,
                random = LegacyRandom(4L),
                reason = MobSpawnReason.MOB_SUMMONED,
            )
        val golem = assertNotNull(EntityNbt.decodeGenerated(forest.main, 30, 55))
        assertNotNull(MobGoals.forType(golem.type))
    }

    @Test
    fun recentSleepAndGolemDetectionMemoryRoundTripWithOfficialBoundaries() {
        val villager = villager()
        villager.lastSleptGameTime = 1_000L
        villager.golemDetectedTicks = 599

        val encoded = EntityNbt.encode(villager)
        val memories = encoded.getCompound("Brain").getCompound("memories")
        assertEquals(1_000L, memories.getCompound("minecraft:last_slept").getLong("value"))
        assertEquals(599L, memories.getCompound("minecraft:golem_detected_recently").getLong("ttl"))

        val restored = assertNotNull(EntityNbt.decode(encoded, 8, villager.protocolType))
        assertEquals(1_000L, restored.lastSleptGameTime)
        assertEquals(599, restored.golemDetectedTicks)
        assertFalse(VillageGolemSpawner.wantsGolem(restored, 1_001L))

        restored.golemDetectedTicks = 0
        assertTrue(VillageGolemSpawner.wantsGolem(restored, 24_999L))
        assertFalse(VillageGolemSpawner.wantsGolem(restored, 25_000L))
    }

    private fun villager(): ServerEntity =
        ServerEntity(
            entityId = 7,
            uuid = Uuid.random(),
            type = Identifier.vanilla("villager"),
            protocolType = 148,
            x = 4.5,
            y = 64.0,
            z = -3.5,
        )
}
