package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.entity.EntityNbt
import org.basaltmc.server.runtime.entity.MobCategory
import org.basaltmc.server.runtime.entity.MobDespawn
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.runtimeEntityMetadata
import org.basaltmc.server.world.ChunkGeneratedEntity
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.protocol.play.SetEntityDataPacket
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** Durable topology and persistence regressions for thunder-created skeleton-horse traps. */
@OptIn(ExperimentalUuidApi::class)
class SkeletonHorseTrapTest {
    @Test
    fun activationBuildsOriginalRiderAndThreePersistentHorsePairs() {
        val original = horse().also { it.skeletonTrap = true }
        var entitySeed = 100L
        val forest =
            SkeletonHorseTrap.buildActivationForest(
                horse = original,
                levelRandom = LegacyRandom(7L),
                horseRandom = LegacyRandom(8L),
                entityRandomFactory = { LegacyRandom(entitySeed++) },
                equipment = SpawnEquipmentFinalizer(null),
                specialMultiplier = 0f,
                biome = Identifier.vanilla("plains"),
                gameTime = 200_000L,
                difficulty = "hard",
            )

        assertFalse(original.skeletonTrap)
        assertTrue(original.tamedHorse)
        assertEquals(0, original.age)
        assertTrue(
            runtimeEntityMetadata(original).changesFrom(null).any { entry ->
                entry is SetEntityDataPacket.Entry.Bytes && entry.index == 18 && entry.value == 2.toByte()
            },
        )
        assertEquals("skeleton", forest.main.type.path)
        assertEquals(original.entityId, forest.externalVehicleLinks.single().vehicleId)
        assertTrue(forest.externalVehicleLinks.single().passenger === forest.main)

        val extraHorses = forest.roots.filter { it.type.path == "skeleton_horse" }
        assertEquals(3, extraHorses.size)
        for (extraHorse in extraHorses) {
            assertTrue(extraHorse.data.getBoolean("PersistenceRequired"))
            assertTrue(extraHorse.data.getBoolean("Tame"))
            assertEquals(0, extraHorse.data.getInt("Age"))
            val motion = assertNotNull(extraHorse.data["Motion"] as? NbtList).filterIsInstance<NbtDouble>()
            assertEquals(3, motion.size)
            assertEquals(0.0, motion[1].value)

            val rider = extraHorse.passengers.single()
            assertEquals("skeleton", rider.type.path)
            assertTrue(rider.data.getBoolean("PersistenceRequired"))
            assertEquals("minecraft:bow", itemId(rider.data, "mainhand"))
            assertEquals("minecraft:iron_helmet", itemId(rider.data, "head"))
        }

        assertTrue(forest.main.data.getBoolean("PersistenceRequired"))
        assertEquals("minecraft:iron_helmet", itemId(forest.main.data, "head"))
        assertEquals(7, forest.roots.sumOf(::treeSize))
    }

    @Test
    fun trapStateRoundTripsAndUsesPostIncrementExpiry() {
        val original =
            horse().also {
                it.skeletonTrap = true
                it.skeletonTrapTime = SkeletonHorseTrap.MAX_LIFE
            }
        val restored = assertNotNull(EntityNbt.decode(EntityNbt.encode(original), 91, original.protocolType))
        assertTrue(restored.skeletonTrap)
        assertEquals(SkeletonHorseTrap.MAX_LIFE, restored.skeletonTrapTime)

        SkeletonHorseTrap.tickLifetime(restored)
        assertTrue(restored.removed)
        assertEquals(SkeletonHorseTrap.MAX_LIFE + 1, restored.skeletonTrapTime)

        val persistent =
            horse().also {
                it.skeletonTrap = true
                it.persistent = true
            }
        SkeletonHorseTrap.tickLifetime(persistent)
        assertFalse(persistent.removed)
        assertEquals(0, persistent.skeletonTrapTime)

        val distant = horse().also { it.idleTicks = 10_000 }
        assertFalse(MobDespawn.shouldDespawn(distant, MobCategory.CREATURE, 200.0, LegacyRandom(1L)))
    }

    private fun horse(): ServerEntity =
        ServerEntity(
            entityId = 42,
            uuid = Uuid.random(),
            type = Identifier.vanilla("skeleton_horse"),
            protocolType = 118,
            x = 12.0,
            y = 70.0,
            z = -4.0,
        )

    private fun itemId(
        data: NbtCompound,
        slot: String,
    ): String = data.getCompound("equipment").getCompound(slot).getString("id")

    private fun treeSize(entity: ChunkGeneratedEntity): Int = 1 + entity.passengers.sumOf(::treeSize)
}
