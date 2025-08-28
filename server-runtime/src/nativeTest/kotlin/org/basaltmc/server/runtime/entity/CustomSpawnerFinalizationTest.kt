package org.basaltmc.server.runtime.entity

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.entity.ai.MobGoals
import org.basaltmc.server.runtime.generatedEntityMetadata
import org.basaltmc.server.runtime.world.CatSpawnContext
import org.basaltmc.server.runtime.world.MobSpawnForest
import org.basaltmc.server.runtime.world.MobSpawnGroupData
import org.basaltmc.server.runtime.world.MobSpawnReason
import org.basaltmc.server.runtime.world.relocateMain
import org.basaltmc.server.runtime.world.writeGeneratedForest
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** Durable spawn-forest regressions shared by the periodic custom spawners. */
class CustomSpawnerFinalizationTest {
    @Test
    fun generatedCatKeepsPostSuperWorldVariantsAndDespawnGrace() {
        val group =
            MobSpawnGroupData().also {
                it.catSpawnContext = CatSpawnContext(moonBrightness = 0f, inBlackCatStructure = true)
            }
        val forest =
            writeGeneratedForest(
                NbtCompound(),
                Identifier.vanilla("cat"),
                groupIndex = 0,
                random = LegacyRandom(0L),
                group = group,
            )
        val data = forest.main.data
        assertEquals("minecraft:all_black", data.getString("variant"))
        // The one-entry PriorityProvider variant draw still advances the stream before sound.
        assertEquals("minecraft:royal", data.getString("sound_variant"))
        assertTrue(
            generatedEntityMetadata("cat", data).any { entry ->
                entry is SetEntityDataPacket.Entry.RegistryId && entry.index == 20 && entry.value == 10
            },
        )

        val live = assertNotNull(EntityNbt.decodeGenerated(forest.main, entityId = 16, protocolType = 16))
        live.tickCount = 2_400
        assertTrue(!MobDespawn.shouldDespawn(live, MobCategory.CREATURE, 200.0, LegacyRandom(0L)))
        live.tickCount++
        assertTrue(MobDespawn.shouldDespawn(live, MobCategory.CREATURE, 200.0, LegacyRandom(0L)))
    }

    @Test
    fun postFinalizationRelocationLeavesSiegeVehicleAtOrigin() {
        val zombie =
            ChunkGeneratedEntity(
                Identifier.vanilla("zombie"),
                0.0,
                0.0,
                0.0,
                0f,
                0f,
            )
        val chicken =
            ChunkGeneratedEntity(
                Identifier.vanilla("chicken"),
                0.0,
                0.0,
                0.0,
                0f,
                0f,
                passengers = listOf(zombie),
            )
        val moved = MobSpawnForest(zombie, listOf(chicken)).relocateMain(40.5, 70.0, -12.5, 90f)
        val vehicle = moved.roots.single()
        assertEquals(0.0, vehicle.x)
        assertEquals(0.0, vehicle.z)
        assertTrue(vehicle.passengers.single() === moved.main)
        assertEquals(40.5, moved.main.x)
        assertEquals(70.0, moved.main.y)
        assertEquals(-12.5, moved.main.z)
        assertEquals(90f, moved.main.yaw)
    }

    @OptIn(ExperimentalUuidApi::class)
    @Test
    fun traderLlamaEventStateLeashAndLinkedLifetimeSurviveMaterialization() {
        val llamaForest =
            writeGeneratedForest(
                NbtCompound(),
                Identifier.vanilla("trader_llama"),
                groupIndex = 0,
                random = LegacyRandom(4L),
                reason = MobSpawnReason.EVENT,
            )
        val finalized = assertNotNull(EntityNbt.decodeGenerated(llamaForest.main, entityId = 2, protocolType = 2))
        assertEquals(0, finalized.age)
        assertEquals(47_999, finalized.despawnDelay)
        assertTrue(llamaForest.main.data.getInt("Strength") in 1..5)
        assertNotNull(MobGoals.forType(finalized.type))
        assertNotNull(MobGoals.forType(Identifier.vanilla("wandering_trader")))

        val trader =
            ServerEntity(1, Uuid.random(), Identifier.vanilla("wandering_trader"), 1, 10.5, 70.0, 10.5).also {
                it.despawnDelay = 3
                it.wanderTarget = intArrayOf(20, 72, 20)
            }
        finalized.leashHolderId = trader.entityId
        finalized.leashHolderUuid = trader.uuid.toString()
        val restoredTrader = assertNotNull(EntityNbt.decode(EntityNbt.encode(trader), 11, 1))
        val restoredLlama = assertNotNull(EntityNbt.decode(EntityNbt.encode(finalized), 12, 2))
        assertTrue(restoredTrader.wanderTarget contentEquals intArrayOf(20, 72, 20))
        assertEquals(trader.uuid.toString(), restoredLlama.leashHolderUuid)
        assertTrue(MobPersistence.keeps(restoredLlama))

        val restored = listOf(restoredTrader, restoredLlama)
        WanderingTraderLifetime.tick(restoredTrader, restored, emptySet())
        WanderingTraderLifetime.tick(restoredLlama, restored, emptySet())
        assertEquals(restoredTrader.entityId, restoredLlama.leashHolderId)
        WanderingTraderLifetime.tick(restoredTrader, restored, emptySet())
        WanderingTraderLifetime.tick(restoredLlama, restored, emptySet())
        assertTrue(restoredLlama.removed)
    }
}
